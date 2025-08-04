# This is not meant to be included by the top-level.
cmake_minimum_required(VERSION 3.16)
project(SMG_DEPS C)

if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Point CMake at any custom modules we may ship
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" "${PROJECT_SOURCE_DIR}/../cmake")

# Prefer our bundled versions of dependencies.
if(DEFINED ENV{DEPS_BUILD_DIR})
  set(DEPS_PREFIX "$ENV{DEPS_BUILD_DIR}/../usr" CACHE PATH "Path prefix for finding dependencies")
else()
  set(DEPS_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/../.deps/usr" CACHE PATH "Path prefix for finding dependencies")
endif()

include(CheckCCompilerFlag)
include(ExternalProject)
include(FindPackageHandleStandardArgs)

# neovim utility functions
include(Deps)

# additional prefix paths
# @hey: check for MSVC
# system detection
if(WIN32)
    message(STATUS "this is a Windows machine")

    # need to add the Visual Studio environment variables
    #& 'C:\Program Files\Microsoft Visual Studio\[VS_VERSION]\VC\Auxiliary\Build\vcvarsall.bat' [ARCHITECTURE]
    #& 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat' x64
    # @hey: hardcoded vcpkg 
    list(INSERT CMAKE_PREFIX_PATH 0 "${CMAKE_CURRENT_SOURCE_DIR}/../vcpkg/installed/x64-windows/lib/cmake/SDL2")
    list(INSERT CMAKE_PREFIX_PATH 0 "${CMAKE_CURRENT_SOURCE_DIR}/../vcpkg/installed/x64-windows/lib/cmake")
    list(INSERT CMAKE_PREFIX_PATH 0 "${CMAKE_CURRENT_SOURCE_DIR}/../vcpkg/installed/x64-windows/include/")
    list(INSERT CMAKE_PREFIX_PATH 0 "${CMAKE_CURRENT_SOURCE_DIR}/../vcpkg/installed/x64-windows/lib/")

    # @hey: add a runtime output for all libraries
    #list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" "${PROJECT_SOURCE_DIR}/../cmake")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/bin CACHE PATH "" FORCE)

endif(WIN32)
if(UNIX)
  message(STATUS "this is a linux machine")
    list(INSERT CMAKE_PREFIX_PATH 0 "/usr/lib/x86_64-linux-gnu/") # for opengl and sdl
endif(UNIX)

list(INSERT CMAKE_PREFIX_PATH 0 "${DEPS_PREFIX}")
list(INSERT CMAKE_PREFIX_PATH 0 "${DEPS_PREFIX}/bin/")
list(INSERT CMAKE_PREFIX_PATH 0 "${DEPS_PREFIX}/include/")
list(INSERT CMAKE_PREFIX_PATH 0 "${DEPS_PREFIX}/lib/")
list(INSERT CMAKE_PREFIX_PATH 0 "${DEPS_PREFIX}/share/")
list(INSERT CMAKE_PREFIX_PATH 0 "${DEPS_PREFIX}/share/cmake/Corrade")

#-------------------------------------------------------------------------------
# User settings
#-------------------------------------------------------------------------------

set(DEPS_IGNORE_SHA FALSE)

# Options
option(USE_BUNDLED "Use bundled dependencies." ON)
option(BUILD_GUI "build the gui library" ${USE_BUNDLED})
option(USE_BUNDLED_SMG "Use the bundled smg." ${USE_BUNDLED})

# @hey: get this working on windows
if(USE_BUNDLED AND MSVC)
    #option(USE_BUNDLED_GETTEXT "Use the bundled version of gettext." ON)
else()
    #option(USE_BUNDLED_GETTEXT "Use the bundled version of gettext." OFF)
endif()

# build dependencies
#option(USE_BUNDLED_SUPERLU "use bundled superlu" ${USE_BUNDLED}) # superlu is not needed
option(USE_BUNDLED_ARMADILLO "use bundled armadillo" ${USE_BUNDLED})

# build superlu
#if(USE_BUNDLED_SUPERLU)
#include(Buildsuperlu)
#endif()

# build armadillo
if(USE_BUNDLED_ARMADILLO)
    include(BuildArmadillo)
endif()

# build jsoncpp
include(Buildjsoncpp)

# for gui
if(USE_BUNDLED_SMG)
    message(STATUS "prefix path: ${CMAKE_PREFIX_PATH}")
    add_subdirectory(smg)
endif()

# if using python
if(USE_BUNDLED_PYBIND11)
    include(Buildpybind11)
endif()

