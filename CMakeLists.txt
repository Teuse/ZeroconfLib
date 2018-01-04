cmake_minimum_required(VERSION 2.8.11)

cmake_policy(SET CMP0063 NEW)

project(ZeroconfLib)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/config)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")

find_package(NIBoost REQUIRED)


#--------------------------------------------------------------------
#--- Collecting all files
#--------------------------------------------------------------------
if (APPLE OR IOS)
    set(FILES_ZC Zeroconf/Browser.h
                 Zeroconf/Browser_bonjour.cpp
                 Zeroconf/Publisher.h
                 Zeroconf/Publisher_bonjour.cpp)

elseif(WIN32)
    set(FILES_ZC Zeroconf/Browser.h
                 Zeroconf/Brower_bonjour.cpp
                 Zeroconf/Publisher.h
                 Zeroconf/Publisher_bonjour.cpp
                 bonjour-sdk/dnssd_clientlib.c
                 bonjour-sdk/dnssd_clientstub.c
                 bonjour-sdk/dnssd_ipc.c)

elseif(UNIX AND NOT APPLE)
    set(FILES_ZC Zeroconf/Browser.h
                 Zeroconf/Browser_avahiclient.cpp
                 Zeroconf/Publisher.h
                 Zeroconf/Publisher_avahiclient.cpp)
else()
    message(FATAL_ERROR "Zeroconf: Unsupported plattform")
endif()

source_group("Zeroconf" FILES ${FILES_ZC})

#--------------------------------------------------------------------
#--- Setup Library
#--------------------------------------------------------------------
add_library(ZeroconfLib STATIC ${FILES_ZC})

set_target_properties(ZeroconfLib PROPERTIES LINKER_LANGUAGE CXX)

##  --------------------------------------------------------------------------------------
##   Dependencies
##  --------------------------------------------------------------------------------------
target_include_directories(ZeroconfLib PUBLIC .)

target_link_libraries(ZeroconfLib PUBLIC boost_signals)

if (APPLE AND NOT IOS)
    target_link_libraries(ZeroconfLib PUBLIC "-framework CoreServices")
elseif (IOS)
    target_link_libraries(ZeroconfLib PUBLIC "-framework MobileCoreServices")
elseif(WIN32)
    find_package(Bonjour REQUIRED)
    target_link_libraries(ZeroconfLib PUBLIC wsock32 ws2_32)
elseif(UNIX AND NOT APPLE)
    find_package(Avahi REQUIRED)
    target_include_directories(ZeroconfLib PUBLIC ./avahi ${AVAHI_INCLUDE_DIRS})
    target_link_libraries(ZeroconfLib ${AVAHI_LIBRARIES})

    #add_definitions(-DQZEROCONF_STATIC)
endif()

