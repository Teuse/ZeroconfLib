#include "Publisher.h"
#include <dns_sd.h>

#include <boost/lockfree/spsc_queue.hpp>

#include <thread>
#include <map>
#include <vector>
#include <string>
#include <iostream>
#include <arpa/inet.h>


#ifndef kDNSServiceFlagsTimeout		// earlier versions of dns_sd.h don't define this constant
	#define	kDNSServiceFlagsTimeout	0x10000
#endif


namespace zeroconf {

//---------------------------------------------------------------------

class Publisher::Impl
{
    using QueueEvent = std::function<void()>;
    using Queue      = boost::lockfree::spsc_queue<QueueEvent>;

public:
	Impl(Publisher* parent);
	~Impl();

    void poll();

	void start(const std::string& name, const std::string& type, const std::string& domain, uint16_t port);
	void stop();

private:

    void servicePublished()           { _parent->_servicePublished(); }
    void error(Error e)               { _parent->_error(e);           }

    void registerCallback(DNSServiceErrorType err);

	Publisher*         _parent   = nullptr;
    Queue              _queue;
	DNSServiceRef      _dnssRef  = nullptr;


    // --- Bonjour Callback

    static void DNSSD_API onRegisterCallback(DNSServiceRef, DNSServiceFlags, DNSServiceErrorType errorCode, const char *,
            const char *, const char *, void *userdata);
};

//---------------------------------------------------------------------

Publisher::Impl::Impl(Publisher *parent)
: _parent(parent)
, _queue(20)
{}

Publisher::Impl::~Impl()
{
    stop();
}

//---------------------------------------------------------------------

void Publisher::Impl::poll()
{
    _queue.consume_all([] (const auto& e) { e(); });
}

//---------------------------------------------------------------------

void Publisher::Impl::start(const std::string& name, const std::string& type, const std::string& domain, uint16_t port)
{
    if (_dnssRef) { error(ZC_SERVICE_REGISTRATION_FAILED); return; }

    //TODO: qFromBigEndian<uint16_t>(port)
    auto err = DNSServiceRegister(&_dnssRef, 0, 0, name.c_str(), type.c_str(), domain.c_str(), NULL, port, 
            0, NULL, (DNSServiceRegisterReply)Publisher::Impl::onRegisterCallback, this);

    if (err != kDNSServiceErr_NoError) {
        stop();
        error(ZC_SERVICE_REGISTRATION_FAILED);
        return;
    }

    std::thread t([this]() 
    {
        auto err = DNSServiceProcessResult(_dnssRef);
        if (err != kDNSServiceErr_NoError) 
        {
           _queue.push([this]{ stop(); error(ZC_SERVICE_REGISTRATION_FAILED); }); 
        }
    });
    t.detach();
}

void Publisher::Impl::stop()
{
    if (_dnssRef) {
        DNSServiceRefDeallocate(_dnssRef);
        _dnssRef = nullptr;
    }
}


//---------------------------------------------------------------------
//--- Bonjour Callbacks
//---------------------------------------------------------------------

void DNSSD_API Publisher::Impl::onRegisterCallback(DNSServiceRef, DNSServiceFlags, DNSServiceErrorType err, 
                                                   const char*, const char*, const char*, void* userdata)
{
	auto* THIS = static_cast<Publisher::Impl*>(userdata);
    THIS->_queue.push([=]
    { 
        THIS->registerCallback(err); 
    });
}

void Publisher::Impl::registerCallback(DNSServiceErrorType err)
{
	if (err == kDNSServiceErr_NoError) {
		servicePublished();
	}
	else {
        stop();
		error(ZC_SERVICE_REGISTRATION_FAILED);
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
