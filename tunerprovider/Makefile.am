AM_CXXFLAGS = -I../libdvbtee -I/usr/include/libhdhomerun

lib_LTLIBRARIES = libdvbtee_tunerprovider.la

libdvbtee_tunerprovider_la_SOURCES = tunerprovider.cpp

EXTRA_DIST = tunerprovider.h

DVBTEE_TUNERPROVIDER_LIBRARY_VERSION=1:0:0

libdvbtee_tunerprovider_la_LDFLAGS = -version-info $(DVBTEE_TUNERPROVIDER_LIBRARY_VERSION)

library_includedir=$(includedir)/dvbtee
library_include_HEADERS = tunerprovider.h
