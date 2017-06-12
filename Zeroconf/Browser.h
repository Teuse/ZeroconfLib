// Copyright (c) 2017  Mathias Roder (teuse@mailbox.org)

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#pragma once
#include <Zeroconf/Service.h>

#include <boost/signals2.hpp>
#include <memory>
#include <string>


namespace zeroconf {

//------------------------------------------------------------------------------

using ServicePtr = std::shared_ptr<Service>;

enum Error 
{
    ZC_BROWSER_FAILED = -1,
    ZC_BROWSER_ALRADY_RUNNING = -2
};

//---------------------------------------------------------------------
    
class Browser
{
   using Connection = boost::signals2::connection; 

public:

	Browser();
	~Browser();

    // Run processing loop
    void poll();

    // Start/Stop Browsing for Services
	void start(const std::string& type);
	void stop();

    // Callbacks
	Connection connectServiceAdded(const std::function<void(ServicePtr)> handler)
    { return _serviceAdded.connect(handler); }

	Connection connectServiceUpdated(const std::function<void(ServicePtr)> handler)
    { return _serviceUpdated.connect(handler); }

	Connection connectServiceRemoved(const std::function<void(ServicePtr)> handler)
    { return _serviceRemoved.connect(handler); }

	Connection connectError(const std::function<void(Error)> handler)
    { return _error.connect(handler); }

private:

	class Impl; friend Impl;
    std::unique_ptr<Impl> _impl;
    
	boost::signals2::signal<void(ServicePtr)>	_serviceAdded;
	boost::signals2::signal<void(ServicePtr)>	_serviceUpdated;
	boost::signals2::signal<void(ServicePtr)>	_serviceRemoved;
	boost::signals2::signal<void(Error)>	    _error;
};

}

