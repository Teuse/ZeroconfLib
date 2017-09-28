# ------------------------------------------------------------------------------
# --- ZeroconfLib
# ------------------------------------------------------------------------------
!ios: error("ZeroconfLib: use this .pro file configuration just for ios!")


TEMPLATE = lib

CONFIG += c++14 \
          staticlib

# ------------------------------------------------------------------------------
# --- Collect files
# ------------------------------------------------------------------------------

INCLUDEPATH += $$_PRO_FILE_PWD_

HEADERS += Zeroconf/Service.h \
           Zeroconf/Publisher.h \
           Zeroconf/Browser.h

SOURCES += Zeroconf/Browser_bonjour.cpp \
           Zeroconf/Publisher_bonjour.cpp
            

# ------------------------------------------------------------------------------
# --- Dependencies
# ------------------------------------------------------------------------------
BOOST_ROOT = $$_PRO_FILE_PWD_/../../boost-ios
isEmpty(BOOST_ROOT){
    error("ZeroconfLib: BOOST_ROOT for ios is not defined")
}

INCLUDEPATH += $$BOOST_ROOT/include

LIBS += -framework CoreServices
