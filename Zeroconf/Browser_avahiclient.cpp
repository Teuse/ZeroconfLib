#include "Browser.h"

#include <avahi-client/client.h>
#include <avahi-common/error.h>
#include <avahi-common/thread-watch.h>
#include <avahi-client/lookup.h>

#include <boost/lockfree/spsc_queue.hpp>

#include <iostream>
#include <map>

namespace zeroconf {

//---------------------------------------------------------------------

class Browser::Impl
{
    using ServiceMap         = std::map<std::string, ServicePtr>;
    using AvahiPollPtr       = std::unique_ptr<AvahiThreadedPoll,   decltype(&avahi_threaded_poll_free)>;
    using AvahiClientPtr     = std::unique_ptr<::AvahiClient,       decltype(&avahi_client_free)>;
    using AvahiBrowserPtr    = std::unique_ptr<AvahiServiceBrowser, decltype(&avahi_service_browser_free)>;

    using QueueEvent = std::function<void()>;
    using Queue      = boost::lockfree::spsc_queue<QueueEvent>;

public:
	Impl(Browser* parent);
	~Impl();
	
    void poll();
	void start(const std::string& type);
	void stop();

private:

    void error(Error e)                 { _parent->_error(e);           }
    void serviceAdded(ServicePtr s)     { _parent->_serviceAdded(s);    }
    void serviceUpdated(ServicePtr s)   { _parent->_serviceUpdated(s);  }
    void serviceRemoved(ServicePtr s)   { _parent->_serviceRemoved(s);  }

    void onBrowseCallback(AvahiIfIndex, AvahiProtocol, AvahiBrowserEvent,
                          std::string name, std::string type, std::string domain);

    void onResolveCallback(AvahiIfIndex, Protocol, AvahiResolverEvent, 
                           std::string name, std::string type, std::string domain, std::string host_name, 
                           std::string address, uint16_t port);

    AvahiPollPtr    _poll       = {nullptr, &avahi_threaded_poll_free};
    AvahiClientPtr  _client     = {nullptr, &avahi_client_free};
	AvahiBrowserPtr _browser    = {nullptr, &avahi_service_browser_free};

	Browser*	    _parent  = nullptr;
    Queue           _queue;
	ServiceMap      _services;


    // --- AVAHI Callback functions

	static void browseCallback(AvahiServiceBrowser*, AvahiIfIndex, AvahiProtocol, AvahiBrowserEvent, 
            const char *name, const char *type, const char *domain, AvahiLookupResultFlags, void* userdata);

	static void resolveCallback(AvahiServiceResolver*, AvahiIfIndex, AvahiProtocol, AvahiResolverEvent, 
            const char *name, const char *type, const char *domain, const char *host_name,  const AvahiAddress*, 
            uint16_t port, AvahiStringList*, AvahiLookupResultFlags, void* userdata);
};

//------------------------------------------------------------------------------

Browser::Impl::Impl(Browser *parent)
: _parent(parent)
, _queue(20)
{
    _poll.reset(avahi_threaded_poll_new());
    if (!_poll) { std::cout << "Browser: Failed to create poll" << std::endl; return; }

    int ret = 0;
    _client.reset(avahi_client_new(avahi_threaded_poll_get(_poll.get()), {}, nullptr, this, &ret));
    if (!_client) { std::cout << "Browser: Start avahi failed with error: " << avahi_strerror(ret) << std::endl; return; }
}

Browser::Impl::~Impl()
{
    stop();
}

//---------------------------------------------------------------------

void Browser::Impl::poll()
{
    _queue.consume_all([] (const auto& e) { e(); });
}

//------------------------------------------------------------------------------

void Browser::Impl::start(const std::string& type)
{
	if (_browser) { error(ZC_BROWSER_ALRADY_RUNNING); return; }

	_browser.reset(avahi_service_browser_new(_client.get(), AVAHI_IF_UNSPEC, AVAHI_PROTO_INET, type.c_str(), NULL, 
                                         AVAHI_LOOKUP_USE_MULTICAST, Browser::Impl::browseCallback, this));
	if (!_browser)
        error(ZC_BROWSER_FAILED);
}

//---------------------------------------------------------------------

void Browser::Impl::stop()
{
    _services.clear();
    _browser.reset(nullptr);
}

//------------------------------------------------------------------------------
// --- AVAHI Callbacks
//------------------------------------------------------------------------------

void Browser::Impl::browseCallback(AvahiServiceBrowser*, AvahiIfIndex interface, AvahiProtocol protocol,
        AvahiBrowserEvent event, const char* name, const char* type, const char* domain,
        AvahiLookupResultFlags flags, void* userdata)
{
	auto* THIS = static_cast<Browser::Impl*>(userdata);
    auto n = std::string(name);
    auto t = std::string(type);
    auto d = std::string(domain);
    THIS->_queue.push([=]
    { 
        if (THIS->_browser)
            THIS->onBrowseCallback(interface, protocol, event, n, t, d); 
    });
}

void Browser::Impl::onBrowseCallback(AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event,
                                      std::string name, std::string type, std::string domain)
{
    switch (event)
    {
        case AVAHI_BROWSER_FAILURE: { stop(); error(ZC_BROWSER_FAILED); break; }
        case AVAHI_BROWSER_NEW: 
        {
            avahi_service_resolver_new(_client.get(), interface, protocol, name.c_str(), type.c_str(), domain.c_str(), 
                                       AVAHI_PROTO_UNSPEC, AVAHI_LOOKUP_USE_MULTICAST, resolveCallback, this);
            break; 
        }
        case AVAHI_BROWSER_REMOVE:
        {
            auto key = name + std::to_string((int)interface);
            if (_services.find(key) != _services.end())
            {            
                auto service = _services[key];
                _services.erase(key);
                serviceRemoved(service);
            }
           break;
        }
        case AVAHI_BROWSER_ALL_FOR_NOW:
        case AVAHI_BROWSER_CACHE_EXHAUSTED:
        default: { break; }
    }
}
//---------------------------------------------------------------------

void Browser::Impl::resolveCallback(AvahiServiceResolver* resolver, AvahiIfIndex interface,
        AvahiProtocol protocol, AvahiResolverEvent event, const char *name,
        const char *type, const char *domain, const char *host_name, const AvahiAddress *address,
        uint16_t port, AvahiStringList *, AvahiLookupResultFlags, void* userdata)
{
	auto* THIS = static_cast<Browser::Impl*>(userdata);
    auto n = std::string(name);
    auto t = std::string(type);
    auto d = std::string(domain);
    auto h = std::string(host_name);

    char a[AVAHI_ADDRESS_STR_MAX];
    avahi_address_snprint(a, sizeof(a), address);
    auto ad = std::string(a);

    THIS->_queue.push([=]
    { 
        auto p = PROTOCOL_UNSPEC;
        if      (protocol == AVAHI_PROTO_INET6) p = PROTOCOL_IPv6;
        else if (protocol == AVAHI_PROTO_INET)  p = PROTOCOL_IPv4;
        THIS->onResolveCallback(interface, p, event, n, t, d, h, ad, port);
    
        avahi_service_resolver_free(resolver);
    });
}

void Browser::Impl::onResolveCallback(AvahiIfIndex interface, Protocol protocol, AvahiResolverEvent event, 
                                     std::string name, std::string type, std::string domain, std::string host_name, 
                                     std::string address, uint16_t port)
{
    if (event == AVAHI_RESOLVER_FOUND) 
    {
        auto key   = name + std::to_string((int)interface);
        auto isNew = _services.find(key) == _services.end();

        if (isNew) {
            _services[key] = std::make_shared<Service>();
        }

        ServicePtr zcs = _services[key];
        zcs->name      = name;
        zcs->type      = type;
        zcs->domain    = domain;
        zcs->host      = host_name;
        zcs->interface = interface;
        zcs->port      = port;
        zcs->protocol  = protocol;
        zcs->address   = address;

        if (isNew) serviceAdded(zcs);
        else       serviceUpdated(zcs);
    }
}

//---------------------------------------------------------------------
//--- Browser
//---------------------------------------------------------------------

Browser::Browser()  { _impl = std::make_unique<Impl>(this); }
Browser::~Browser() = default;

//---------------------------------------------------------------------

void Browser::poll()                            { _impl->poll();      }
void Browser::start(const std::string& type)	{ _impl->start(type); }
void Browser::stop() 							{ _impl->stop();      }

}

