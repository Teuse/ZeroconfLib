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
#include <boost/signals2.hpp>

#include <memory>
#include <string>


namespace zeroconf {

//------------------------------------------------------------------------------

class Publisher
{
   using Connection = boost::signals2::connection; 

public:

    enum Error 
    {
        ZC_SERVICE_REGISTRATION_FAILED = -1,
        ZC_SERVICE_NAME_COLLISION      = -2,
    };

	Publisher();
	~Publisher();

    // Run processing loop
    void poll();

    // Start/Stop Publishing a Service
	void start(const std::string& name, const std::string& type, const std::string& domain, uint16_t port);
	void stop();

	Connection connectServicePublished(const std::function<void()> handler)
    { return _servicePublished.connect(handler); }

	Connection connectError(const std::function<void(Error)> handler)
    { return _error.connect(handler); }

private:

	class Impl; friend Impl;
    std::unique_ptr<Impl> _impl;
	
	boost::signals2::signal<void()>			_servicePublished;
	boost::signals2::signal<void(Error)>	_error;
};

}

