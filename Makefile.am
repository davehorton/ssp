INCLUDES= -I/usr/local/include -I${srcdir}/deps/boost_1_52_0 -I${srcdir}/deps/sofia-sip-1.12.11/libsofia-sip-ua/su -I${srcdir}/deps/sofia-sip-1.12.11/libsofia-sip-ua/nta -I${srcdir}/deps/sofia-sip-1.12.11/libsofia-sip-ua/sip -I${srcdir}/deps/sofia-sip-1.12.11/libsofia-sip-ua/msg -I${srcdir}/deps/sofia-sip-1.12.11/libsofia-sip-ua/url
AM_LDFLAGS= -L/usr/local/lib  
LDADD= -lboost_thread -lpthread -lssl
bin_PROGRAMS= ssp fsspoofer
ssp_SOURCES= main.cpp ssp-controller.cpp ssp-config.cpp fs-monitor.cpp fs-instance.cpp fs-message.cpp fs-exception.cpp sip-inbound-call.cpp ssp.cpp
ssp_CPPFLAGS=-D_REENTRANT
ssp_LDADD= ${srcdir}/deps/sofia-sip-1.12.11/libsofia-sip-ua/.libs/libsofia-sip-ua.a ${srcdir}/deps/boost_1_52_0/stage/lib/libboost_thread.a ${srcdir}/deps/boost_1_52_0/stage/lib/libboost_system.a ${srcdir}/deps/boost_1_52_0/stage/lib/libboost_log.a -lpthread -lssl -lcrypto
fsspoofer_SOURCES= fs-spoofer.cpp
fsspoofer_CPPFLAGS=-D_REENTRANT
fsspoofer_LDADD= ${srcdir}/deps/boost_1_52_0/stage/lib/libboost_system.a -lpthread -lssl -lcrypto

if LINUX
ssp_LDADD += -lrt
fsspoofer_LDADD += -lrt
endif

#clean-local:
#	cd ${srcdir}/deps/boost_1_52_0 && b2 --clean

${srcdir}/deps/boost_1_52_0/stage/lib/libboost_thread.a ${srcdir}/deps/boost_1_52_0/stage/lib/libboost_system.a ${srcdir}/deps/boost_1_52_0/stage/lib/libboost_log.a:
	cd ${srcdir}/deps/boost_1_52_0 && ./b2 stage

${srcdir}/deps/sofia-sip-1.12.11/libsofia-sip-ua/.libs/libsofia-sip-ua.a:
	cd ${srcdir}/deps/sofia-sip-1.12.11 && $(MAKE)
