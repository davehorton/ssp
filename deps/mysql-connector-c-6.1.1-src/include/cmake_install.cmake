# Install script for directory: /Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include

# Set the install prefix
IF(NOT DEFINED CMAKE_INSTALL_PREFIX)
  SET(CMAKE_INSTALL_PREFIX "/usr/local/mysql")
ENDIF(NOT DEFINED CMAKE_INSTALL_PREFIX)
STRING(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
IF(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  IF(BUILD_TYPE)
    STRING(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  ELSE(BUILD_TYPE)
    SET(CMAKE_INSTALL_CONFIG_NAME "RelWithDebInfo")
  ENDIF(BUILD_TYPE)
  MESSAGE(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
ENDIF(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)

# Set the component getting installed.
IF(NOT CMAKE_INSTALL_COMPONENT)
  IF(COMPONENT)
    MESSAGE(STATUS "Install component: \"${COMPONENT}\"")
    SET(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  ELSE(COMPONENT)
    SET(CMAKE_INSTALL_COMPONENT)
  ENDIF(COMPONENT)
ENDIF(NOT CMAKE_INSTALL_COMPONENT)

IF(NOT CMAKE_INSTALL_COMPONENT OR "${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development")
  FILE(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include" TYPE FILE FILES
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/mysql.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/mysql_com.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/mysql_time.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/my_list.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/my_alloc.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/typelib.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/my_dbug.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/m_string.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/my_sys.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/my_xml.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/mysql_embed.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/my_pthread.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/decimal.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/errmsg.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/my_global.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/my_net.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/my_getopt.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/sslopt-longopts.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/my_dir.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/sslopt-vars.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/sslopt-case.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/sql_common.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/keycache.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/m_ctype.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/my_attribute.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/my_compiler.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/mysql_com_server.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/my_byteorder.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/byte_order_generic.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/byte_order_generic_x86.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/byte_order_generic_x86_64.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/little_endian.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/big_endian.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/mysql_version.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/my_config.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/mysqld_ername.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/mysqld_error.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/sql_state.h"
    )
ENDIF(NOT CMAKE_INSTALL_COMPONENT OR "${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development")

IF(NOT CMAKE_INSTALL_COMPONENT OR "${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development")
  FILE(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/mysql" TYPE DIRECTORY FILES "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/include/mysql/" REGEX "/[^/]*\\.h$" REGEX "/psi\\_abi[^/]*$" EXCLUDE)
ENDIF(NOT CMAKE_INSTALL_COMPONENT OR "${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development")

