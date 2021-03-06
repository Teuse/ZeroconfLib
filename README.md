# ZeroconfLib
ZeroconfLib is a wrapper for Bonjour and Avahi to have a cross-platform library for zeroconf networks.

**Details:**
* Used bonjour on Mac, iOS and Win
* Uses Avahi on Linux (and maybe Android in the future)

### Build & Install
```
cmake ../ZeroconfLib
make
```
**Note:**
The *install* step is not implemented yet! Let me know if you need it :)

**Build for iOS**
For iOS, you need a cross compiled version of boost. See also https://github.com/faithfracture/Apple-Boost-BuildScript.
Put the __include__ and __lib__ directory of boost to "/path/to/your-project/../boost-ios".

cmake -DIOS=ON -DBOOST_IOS_ROOT="/path/to/your/boost_root/" ../ZeroconfLib
make

__BOOST_IOS_ROOT__ should contain the __include__and __lib__ directories with all boost sources/libs. 

### Documentation
Publish a zeroconf service:
```cpp
#include <Zeroconf/Publisher.h>
#include <string>
#include <iostream>

void publish(unsigned port, std::string serviceType)
{
    zeroconf::Publisher publisher;
    publisher.connectError([this](zeroconf::Error error)
    { std::cout << "Publisher error: " << error << std::endl; });

    publisher.start("MyAppName", serviceType, "local", port);
}
```
Browse for services:
```cpp
#include <Zeroconf/Browser.h>
#include <string>
#include <iostream>

void browse(std::string serviceType)
{
    zeroconf::Browser browser;
    browser.connectServiceAdded([this](zeroconf::ServicePtr s)
    { std::cout << "Service found: " << s->name << std::endl; });        
    browser.connectServiceUpdated([this](zeroconf::ServicePtr s)
    { std::cout << "Service updated: " << s->name << std::endl; });
    browser.connectError([this](zeroconf::Error error)
    { std::cout << "Publisher error: " << error << std::endl; });

    browser.start(brew::cfg::serviceType);
}
```
To run the internal event loop, you must call the following function from your application loop:
```cpp
publisher.poll();
```
Respectively for the Browser:
```cpp
browser.poll();
```

### Dependencies
* C++11
* Bonjour on Mac
* Avahi on Linux
