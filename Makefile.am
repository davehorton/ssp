MYVERSION=$(shell git describe)
INCLUDES= -I/usr/local/include -I${srcdir}/deps/boost_1_55_0 -I${srcdir}/deps/sofia-sip-1.12.11/libsofia-sip-ua/su -I${srcdir}/deps/sofia-sip-1.12.11/libsofia-sip-ua/nta -I${srcdir}/deps/sofia-sip-1.12.11/libsofia-sip-ua/sip -I${srcdir}/deps/sofia-sip-1.12.11/libsofia-sip-ua/msg -I${srcdir}/deps/sofia-sip-1.12.11/libsofia-sip-ua/url -I${srcdir}/deps/mysql-connector-c++-1.1.3
AM_LDFLAGS= -L/usr/local/lib -L/usr/local/mysql/lib
LDADD= -lboost_thread -lpthread -lssl
bin_PROGRAMS= ssp fsspoofer check-ssp duptester
ssp_SOURCES= main.cpp ssp-controller.cpp ssp-config.cpp fs-monitor.cpp fs-instance.cpp fs-message.cpp fs-exception.cpp sip-inbound-call.cpp ssp.cpp nagios-connector.cpp cdr-writer.cpp
ssp_CPPFLAGS=-D_REENTRANT -DSSP_VERSION=\"$(MYVERSION)\"
ssp_LDADD= ${srcdir}/deps/sofia-sip-1.12.11/libsofia-sip-ua/.libs/libsofia-sip-ua.a ${srcdir}/deps/boost_1_55_0/stage/lib/libboost_thread.a ${srcdir}/deps/boost_1_55_0/stage/lib/libboost_system.a ${srcdir}/deps/boost_1_55_0/stage/lib/libboost_log.a ${srcdir}/deps/mysql-connector-c++-1.1.3/driver/libmysqlcppconn-static.a -lmysqlclient -lpthread -lssl -lcrypto -lz
fsspoofer_SOURCES= fs-spoofer.cpp
fsspoofer_CPPFLAGS=-D_REENTRANT
fsspoofer_LDADD= ${srcdir}/deps/boost_1_55_0/stage/lib/libboost_system.a -lpthread -lssl -lcrypto
check_ssp_SOURCES= check-ssp.cpp
check_ssp_CPPFLAGS=-D_REENTRANT
check_ssp_LDADD= ${srcdir}/deps/boost_1_55_0/stage/lib/libboost_system.a -lpthread -lssl -lcrypto
duptester_SOURCES= duplicate-tester.cpp
duptester_CPPFLAGS=-D_REENTRANT
duptester_LDADD= ${srcdir}/deps/sofia-sip-1.12.11/libsofia-sip-ua/.libs/libsofia-sip-ua.a  ${srcdir}/deps/boost_1_55_0/stage/lib/libboost_system.a ${srcdir}/deps/boost_1_55_0/stage/lib/libboost_thread.a  -lpthread -lssl -lcrypto

if LINUX
ssp_LDADD += -lrt
fsspoofer_LDADD += -lrt
check_ssp_LDADD += -lrt
duptester_LDADD += -lrt
endif

#clean-local:
#	cd ${srcdir}/deps/boost_1_52_0 && b2 --clean

${srcdir}/deps/boost_1_55_0/stage/lib/libboost_thread.a ${srcdir}/deps/boost_1_55_0/stage/lib/libboost_system.a ${srcdir}/deps/boost_1_55_0/stage/lib/libboost_log.a:
	cd ${srcdir}/deps/boost_1_55_0 && ./bootstrap.sh && ./bjam install &&./b2 stage

${srcdir}/deps/sofia-sip-1.12.11/libsofia-sip-ua/.libs/libsofia-sip-ua.a:
	cd ${srcdir}/deps/sofia-sip-1.12.11 && $(MAKE)

libmysqlclient.so:
	cd ${srcdir}/deps/mysql-connector-c-6.1.1-src && cmake -G "Unix Makefiles" && make && make install

${srcdir}/deps/mysql-connector-c++-1.1.3/driver/libmysqlcppconn-static.a:
	cd ${srcdir}/deps/mysql-connector-c++-1.1.3 && cmake . && make && make install