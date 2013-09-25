# Install script for directory: /Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/cppconn

# Set the install prefix
IF(NOT DEFINED CMAKE_INSTALL_PREFIX)
  SET(CMAKE_INSTALL_PREFIX "/usr/local")
ENDIF(NOT DEFINED CMAKE_INSTALL_PREFIX)
STRING(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
IF(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  IF(BUILD_TYPE)
    STRING(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  ELSE(BUILD_TYPE)
    SET(CMAKE_INSTALL_CONFIG_NAME "")
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

IF(NOT CMAKE_INSTALL_COMPONENT OR "${CMAKE_INSTALL_COMPONENT}" STREQUAL "Unspecified")
  FILE(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cppconn" TYPE FILE FILES
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/cppconn/build_config.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/cppconn/config.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/cppconn/connection.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/cppconn/datatype.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/cppconn/driver.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/cppconn/exception.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/cppconn/metadata.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/cppconn/parameter_metadata.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/cppconn/prepared_statement.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/cppconn/resultset.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/cppconn/resultset_metadata.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/cppconn/statement.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/cppconn/sqlstring.h"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/cppconn/warning.h"
    )
ENDIF(NOT CMAKE_INSTALL_COMPONENT OR "${CMAKE_INSTALL_COMPONENT}" STREQUAL "Unspecified")

