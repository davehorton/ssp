# Install script for directory: /Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3

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
  FILE(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/." TYPE FILE OPTIONAL FILES
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/README"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/INSTALL"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/COPYING"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/Licenses_for_Third-Party_Components.txt"
    "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/ANNOUNCEMENT"
    )
ENDIF(NOT CMAKE_INSTALL_COMPONENT OR "${CMAKE_INSTALL_COMPONENT}" STREQUAL "Unspecified")

IF(NOT CMAKE_INSTALL_COMPONENT OR "${CMAKE_INSTALL_COMPONENT}" STREQUAL "Unspecified")
  FILE(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/." TYPE FILE OPTIONAL FILES "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/BUILDINFO")
ENDIF(NOT CMAKE_INSTALL_COMPONENT OR "${CMAKE_INSTALL_COMPONENT}" STREQUAL "Unspecified")

IF(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  INCLUDE("/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/cppconn/cmake_install.cmake")
  INCLUDE("/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/driver/cmake_install.cmake")
  INCLUDE("/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/examples/cmake_install.cmake")
  INCLUDE("/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/test/cmake_install.cmake")
  INCLUDE("/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/test/framework/cmake_install.cmake")
  INCLUDE("/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/test/CJUnitTestsPort/cmake_install.cmake")
  INCLUDE("/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/test/unit/cmake_install.cmake")

ENDIF(NOT CMAKE_INSTALL_LOCAL_ONLY)

IF(CMAKE_INSTALL_COMPONENT)
  SET(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
ELSE(CMAKE_INSTALL_COMPONENT)
  SET(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
ENDIF(CMAKE_INSTALL_COMPONENT)

FILE(WRITE "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/${CMAKE_INSTALL_MANIFEST}" "")
FOREACH(file ${CMAKE_INSTALL_MANIFEST_FILES})
  FILE(APPEND "/Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/${CMAKE_INSTALL_MANIFEST}" "${file}\n")
ENDFOREACH(file)
