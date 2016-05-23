cmake_minimum_required(VERSION 2.8)

project(gdalwrap)
set(PACKAGE_NAME ${CMAKE_PROJECT_NAME})
set(PACKAGE_VERSION "0.1.10")

option(BUILD_TESTS  "Build tests" ON)
option(BUILD_TOOLS "Build tools" OFF)

# C++11 for GCC 4.6
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/CMakeModules")

# Find GDAL ( export GDAL_ROOT=$prefix )
find_package(GDAL REQUIRED)

include_directories(include)
include_directories(${GDAL_INCLUDE_DIRS})

# Load the 'install_pkg_config_file' macro
include(InstallPkgConfigFile)

# RPath management http://www.cmake.org/Wiki/CMake_RPATH_handling
set(CMAKE_SKIP_BUILD_RPATH  FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib" isSystemDir)
if("${isSystemDir}" STREQUAL "-1")
	set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
endif("${isSystemDir}" STREQUAL "-1")

# Library and binary
add_subdirectory(src)

if (BUILD_TOOLS)
    add_subdirectory(tools)
endif(BUILD_TOOLS)

if (BUILD_TESTS)
  include( CTest )
  add_subdirectory(test)
endif(BUILD_TESTS)

# Install headers
file(GLOB gdalwrap_HDRS "include/gdalwrap/*.hpp")
install(FILES ${gdalwrap_HDRS} DESTINATION include/gdalwrap)

#########################################################################
### Uninstall ###
#########################################################################
configure_file("${CMAKE_HOME_DIRECTORY}/CMakeModules/cmake_uninstall.cmake.in" "cmake_uninstall.cmake" IMMEDIATE @ONLY)
add_custom_target(uninstall "${CMAKE_COMMAND}" -P "cmake_uninstall.cmake")
