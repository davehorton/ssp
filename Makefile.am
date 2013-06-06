INCLUDES= -I/usr/local/include -I${srcdir}/deps/boost_1_52_0 -I${srcdir}/deps/sofia-sip-1.12.11/libsofia-sip-ua
AM_LDFLAGS= -L/usr/local/lib  
LDADD= -lboost_thread -lpthread -lssl
bin_PROGRAMS= ssp
ssp_SOURCES= main.cpp sip-agent.cpp sip-call-leg.cpp sip-b2b-call.cpp ssp-controller.cpp sip-server.cpp ssp-config.cpp ssp.cpp
ssp_CPPFLAGS=-D_REENTRANT
ssp_LDADD= ${srcdir}/deps/sofia-sip-1.12.11/libsofia-sip-ua/.libs/libsofia-sip-ua.a ${srcdir}/deps/boost_1_52_0/stage/lib/libboost_thread.a ${srcdir}/deps/boost_1_52_0/stage/lib/libboost_system.a ${srcdir}/deps/boost_1_52_0/stage/lib/libboost_log.a -lpthread -lssl -lcrypto

#clean-local:
#	cd ${srcdir}/deps/boost_1_52_0 && b2 --clean

${srcdir}/deps/boost_1_52_0/stage/lib/libboost_thread.a ${srcdir}/deps/boost_1_52_0/stage/lib/libboost_system.a ${srcdir}/deps/boost_1_52_0/stage/lib/libboost_log.a:
	cd ${srcdir}/deps/boost_1_52_0 && b2 stage

${srcdir}/deps/sofia-sip-1.12.11/libsofia-sip-ua/.libs/libsofia-sip-ua.a:
	cd ${srcdir}/deps/sofia-sip-1.12.11 && $(MAKE)