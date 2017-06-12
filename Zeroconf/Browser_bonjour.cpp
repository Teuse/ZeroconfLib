#include "Browser.h"
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

namespace convert 
{
    Protocol getProtokol(const struct sockaddr *res)
    {
        switch(res->sa_family) 
        {
            case AF_INET:  { return PROTOCOL_IPv4; break; }
            case AF_INET6: { return PROTOCOL_IPv6; break; }
            default:       { break; }
        }
        return PROTOCOL_UNSPEC; 
    }

    std::string getAddress(const struct sockaddr* res)
    {
        auto p = getProtokol(res);
        if (p == PROTOCOL_UNSPEC) return "";

        auto is4 = (p == PROTOCOL_IPv4);
        auto len = is4 ? INET_ADDRSTRLEN : INET6_ADDRSTRLEN;
        auto s   = std::string(); s.resize(len);

        if (is4) {
            auto* sin = (struct sockaddr_in *)res;
            inet_ntop(res->sa_family, &(sin->sin_addr),  &s[0], len);
        }
        else {
            auto* sin = (struct sockaddr_in6 *)res;
            inet_ntop(res->sa_family, &(sin->sin6_addr), &s[0], len);
        }

        auto c = s.find('\0');
        if (c != std::string::npos) 
            s.resize(c);

        return s;
    }
}

//---------------------------------------------------------------------

class Browser::Impl
{
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

	void resolve();
    void stopResolve(bool all=false);

    void browseCallback(DNSServiceFlags, uint32_t interface, std::string name, std::string type, std::string domain);
    void resolverCallback(uint32_t interface, std::string hostName, uint16_t port);
    void addressCallback(DNSServiceFlags, uint32_t interface, std::string address, Protocol);

	Browser*           _parent = nullptr;
    Queue              _queue;
	DNSServiceRef      _browser  = nullptr;
	DNSServiceRef      _resolver = nullptr;

	std::map<std::string, ServicePtr> _services;
    std::vector<ServicePtr>           _work;


    // --- Bonjour Callbacks

    static void DNSSD_API onBrowseCallback(DNSServiceRef, DNSServiceFlags flags,uint32_t, DNSServiceErrorType err, const char *name,
            const char *type, const char *domain, void *userdata);

    static void DNSSD_API onResolverCallback(DNSServiceRef, DNSServiceFlags, uint32_t, DNSServiceErrorType err, const char *,
            const char *hostName, uint16_t port, uint16_t txtLen,	const char * txtRecord, void *userdata);

    static void DNSSD_API onAddressCallback(DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex,
            DNSServiceErrorType err, const char *hostName, const struct sockaddr* address, uint32_t ttl, void *userdata);
};

//---------------------------------------------------------------------

Browser::Impl::Impl(Browser *parent)
: _parent(parent)
, _queue(20)
{}

Browser::Impl::~Impl()
{
    stop();
    stopResolve(true);
}

//---------------------------------------------------------------------

void Browser::Impl::poll()
{
    _queue.consume_all([] (const auto& e) { e(); });
}

//---------------------------------------------------------------------

void Browser::Impl::start(const std::string& type)
{
	if (_browser) { error(ZC_BROWSER_ALRADY_RUNNING); return; }

	auto err  = DNSServiceBrowse(&_browser, 0, 0, type.c_str(), 0, 
                                 (DNSServiceBrowseReply) Browser::Impl::onBrowseCallback, this);

    if (err != kDNSServiceErr_NoError) {
        stop();
        error(ZC_BROWSER_FAILED);
        return;
    }

    std::thread t([this]() 
    {
        auto err = DNSServiceProcessResult(_browser);
        if (err != kDNSServiceErr_NoError) 
        {
           _queue.push([this]{ stop(); error(ZC_BROWSER_FAILED); }); 
        }
    });
    t.detach();
}

void Browser::Impl::stop()
{
    if (_browser) {
        DNSServiceRefDeallocate(_browser);
        _browser = nullptr;
        _services.clear();
    }
}

//---------------------------------------------------------------------

void Browser::Impl::resolve()
{
    if (_work.empty() || _resolver) return;

    auto service = _work.front();
    auto err     = DNSServiceResolve(&_resolver, kDNSServiceFlagsTimeout, service->interface, service->name.c_str(), 
                                     service->type.c_str(), service->domain.c_str(), (DNSServiceResolveReply) Browser::Impl::onResolverCallback, this);

    if (err != kDNSServiceErr_NoError) {
        stopResolve();
        return;
    }

    std::thread t([this]() 
    {
        auto err = DNSServiceProcessResult(_resolver);
        if (err != kDNSServiceErr_NoError)
        {
            _queue.push([this]{ stopResolve(); }); 
        }
    });
    t.detach();
}

void Browser::Impl::stopResolve(bool all)
{
    if (!_resolver)
    {
        DNSServiceRefDeallocate(_resolver);
        _resolver = nullptr;
        
        if (all) 
            _work.clear();
        else     
        {
            _work.erase(_work.begin());
            resolve();
        }
    }
}


//---------------------------------------------------------------------
//--- DNSSD Callbacks
//---------------------------------------------------------------------

void DNSSD_API Browser::Impl::onBrowseCallback(DNSServiceRef, DNSServiceFlags flags,	uint32_t interfaceIndex, DNSServiceErrorType err, 
                              const char *name, const char *type, const char *domain, void *userdata)
{
	auto* THIS = static_cast<Browser::Impl*>(userdata);
    auto n = std::string(name);
    auto t = std::string(type);
    auto d = std::string(domain);
    THIS->_queue.push([=]
    { 
	    if (err != kDNSServiceErr_NoError)  { THIS->stop(); THIS->error(ZC_BROWSER_FAILED); }
        else                                { THIS->browseCallback(flags, interfaceIndex, n, t, d); }
    });
}

void Browser::Impl::browseCallback(DNSServiceFlags flags, uint32_t interface, 
                                    std::string name, std::string type, std::string domain)
{
    auto key   = name + std::to_string(interface);
    auto isNew = _services.find(key) == _services.end();
    if (flags & kDNSServiceFlagsAdd) 
    {
        if (isNew) 
        {
            auto zcs = std::make_shared<Service>();
            zcs->name = name;
            zcs->type = type;
            zcs->domain = domain;
            zcs->interface = interface;
            _work.push_back(zcs);
            resolve();
        }
    }
    else if (!isNew) 
    {
        serviceRemoved(_services[key]);
        _services.erase(key);
    }
}

//---------------------------------------------------------------------

void DNSSD_API Browser::Impl::onResolverCallback(DNSServiceRef, DNSServiceFlags, uint32_t interfaceIndex, DNSServiceErrorType err, 
                                const char*, const char* hostName, uint16_t port, uint16_t, const char*, void* userdata)
{
	auto* THIS = static_cast<Browser::Impl*>(userdata);
    auto h = std::string(hostName);
    THIS->_queue.push([=]
    { 
	    if (err != kDNSServiceErr_NoError) { THIS->stopResolve(); }
        else                               { THIS->resolverCallback(interfaceIndex, h, port); }
    });
}

void Browser::Impl::resolverCallback(uint32_t interfaceIndex, std::string hostName, uint16_t port)
{
    auto service = _work.front();
	// service->port = qFromBigEndian<uint16_t>(port);
	service->port = port;

	auto err = DNSServiceGetAddrInfo(&_resolver, kDNSServiceFlagsForceMulticast, interfaceIndex, kDNSServiceProtocol_IPv4, hostName.c_str(), 
                                (DNSServiceGetAddrInfoReply) Browser::Impl::onAddressCallback, this);

	if (err != kDNSServiceErr_NoError) {
        stopResolve();
        return;
    }

    std::thread t([this]() 
    {
        auto err = DNSServiceProcessResult(_resolver);
        if (err != kDNSServiceErr_NoError) 
        {
            _queue.push([this]{ stopResolve(); }); 
        }
    });
    t.detach();
}

//---------------------------------------------------------------------

void DNSSD_API Browser::Impl::onAddressCallback(DNSServiceRef,DNSServiceFlags flags, uint32_t interface, DNSServiceErrorType err, const char* name,
		                    const struct sockaddr* address, uint32_t, void* userdata)
{
	auto* THIS = static_cast<Browser::Impl*>(userdata);

    auto p = convert::getProtokol(address);
    auto a = convert::getAddress(address);

    THIS->_queue.push([=]
    { 
	    if (err != kDNSServiceErr_NoError) { THIS->stopResolve(); }
        else                               { THIS->addressCallback(flags, interface, a, p); }
    });
}

void Browser::Impl::addressCallback(DNSServiceFlags flags, uint32_t interfaceIndex, std::string address, Protocol protokol)
{
    if ((flags & kDNSServiceFlagsAdd) != 0) 
    {
        auto service = _work.front();

        service->protocol = protokol;
        service->address  = address;

        auto key   = service->name + std::to_string(interfaceIndex);
        auto isNew = _services.find(key) == _services.end();

        if (isNew) {
            _services[key] = service;
            serviceAdded(service);
        }
        else
            serviceUpdated(service);
    }

    stopResolve();
}

//---------------------------------------------------------------------
//--- Browser
//---------------------------------------------------------------------

Browser::Browser() 	{ _impl = std::make_unique<Impl>(this); }
Browser::~Browser() = default;

//---------------------------------------------------------------------

void Browser::poll()                            { _impl->poll(); }
void Browser::start(const std::string& type)	{ _impl->start(type); }
void Browser::stop() 							{ _impl->stop(); }

} 
