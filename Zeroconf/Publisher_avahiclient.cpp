#include "Publisher.h"

#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/error.h>
#include <avahi-common/thread-watch.h>
#include <avahi-client/lookup.h>

#include <boost/lockfree/spsc_queue.hpp>

#include <iostream>
#include <map>

namespace zeroconf {

//---------------------------------------------------------------------

class Publisher::Impl
{
    using AvahiPollPtr       = std::unique_ptr<AvahiThreadedPoll,   decltype(&avahi_threaded_poll_free)>;
    using AvahiClientPtr     = std::unique_ptr<::AvahiClient,       decltype(&avahi_client_free)>;
    using AvahiEntryGroupPtr = std::unique_ptr<AvahiEntryGroup,     decltype(&avahi_entry_group_free)>;

    using QueueEvent = std::function<void()>;
    using Queue      = boost::lockfree::spsc_queue<QueueEvent>;


public:
	Impl(Publisher* parent);
	~Impl();
	
    void poll();

	void start(const std::string& name, const std::string& type, const std::string& domain, unsigned port);
	void stop();
	

private:

    void servicePublished()           { _parent->_servicePublished(); }
    void error(Error e)               { _parent->_error(e);           }

    void onGroupCallback(AvahiEntryGroupState state);


    AvahiPollPtr       _poll       = {nullptr, &avahi_threaded_poll_free};
    AvahiClientPtr     _client     = {nullptr, &avahi_client_free};
    AvahiEntryGroupPtr _group      = {nullptr, &avahi_entry_group_free};

	Publisher*	    _parent  = nullptr;
    Queue           _queue;
    std::string     _name;
    std::string     _type;
    std::string     _domain;
    unsigned        _port;


    // --- AVAHI Callback

	static void groupCallback(AvahiEntryGroup* g, AvahiEntryGroupState state, AVAHI_GCC_UNUSED void *userdata);
};

//------------------------------------------------------------------------------

Publisher::Impl::Impl(Publisher *parent)
: _parent(parent)
, _queue(20)
{
    _poll.reset(avahi_threaded_poll_new());
    if (!_poll) { std::cout << "Publisher: Failed to create poll" << std::endl; return; }

  int ret = 0;
  _client.reset(avahi_client_new(avahi_threaded_poll_get(_poll.get()), {}, nullptr, this, &ret));
  if (!_client) { std::cout << "Publisher: Start avahi failed with error: " << avahi_strerror(ret) << std::endl; return; }
}

Publisher::Impl::~Impl()
{}

//---------------------------------------------------------------------

void Publisher::Impl::poll()
{
    _queue.consume_all([] (const auto& e) { e(); });
}

//------------------------------------------------------------------------------

void Publisher::Impl::start(const std::string& name, const std::string& type, const std::string& domain, unsigned port)
{
	if (_group) {
        error(ZC_SERVICE_REGISTRATION_FAILED);
		return;
	}

    _name   = name;
    _type   = type;
    _domain = domain;
    _port   = port;

    _group.reset(avahi_entry_group_new(_client.get(), Publisher::Impl::groupCallback, this));
	if (!_group)
        error(ZC_SERVICE_REGISTRATION_FAILED);
}

void Publisher::Impl::stop()
{
    _group.reset(nullptr);
}

//------------------------------------------------------------------------------
// --- AVAHI Callbacks
//------------------------------------------------------------------------------

void Publisher::Impl::groupCallback(AvahiEntryGroup* group, AvahiEntryGroupState state, AVAHI_GCC_UNUSED void *userdata)
{
	auto* THIS = static_cast<Publisher::Impl*>(userdata);
    THIS->_queue.push([=]
    { 
        if (THIS->_group)
            THIS->onGroupCallback(state); 
    });
}

void Publisher::Impl::onGroupCallback(AvahiEntryGroupState state)
{
    switch (state) 
    {
        case AVAHI_ENTRY_GROUP_ESTABLISHED: { servicePublished(); break; }
        case AVAHI_ENTRY_GROUP_COLLISION:   { stop(); error(ZC_SERVICE_NAME_COLLISION); break; }
        case AVAHI_ENTRY_GROUP_FAILURE:     { stop(); error(ZC_SERVICE_REGISTRATION_FAILED); break; }
        case AVAHI_ENTRY_GROUP_REGISTERING: { break; }
        case AVAHI_ENTRY_GROUP_UNCOMMITED:  
        {
            auto ret = avahi_entry_group_add_service(_group.get(), AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 
                                                        AVAHI_PUBLISH_UPDATE, _name.c_str(), _type.c_str(), _domain.c_str(), NULL, _port, NULL);
            if (ret < 0) {
                _group.reset(nullptr); error(ZC_SERVICE_REGISTRATION_FAILED); break; 
            }
            else
            {
                ret = avahi_entry_group_commit(_group.get());
                if (ret < 0) {
                    _group.reset(nullptr); error(ZC_SERVICE_REGISTRATION_FAILED); break; 
                }
            }
            break; 
        }
    }
}


//---------------------------------------------------------------------
//--- Publisher
//---------------------------------------------------------------------

Publisher::Publisher() 	{ _impl = std::make_unique<Impl>(this); }
Publisher::~Publisher()   {}

//---------------------------------------------------------------------

void Publisher::start(const std::string& name, const std::string& type, const std::string& domain, uint16_t port)
{ 
	_impl->start(name, type, domain, port); 
}

void Publisher::stop()    { _impl->stop(); }
void Publisher::poll()    { _impl->poll(); }
}

