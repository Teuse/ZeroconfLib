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
#include <string>

//-----------------------------------------------------------------------------

namespace zeroconf 
{

    enum Protocol
    {
        PROTOCOL_IPv4,
        PROTOCOL_IPv6,
        PROTOCOL_UNSPEC
    };

    struct Service 
    {
        std::string	    name;
        std::string	    type;
        std::string	    domain;
        std::string	    host;
        Protocol        protocol;
        std::string     address;
        uint32_t        interface;
        uint16_t        port;
    };

}
