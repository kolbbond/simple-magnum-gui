cmake_minimum_required(VERSION 3.1)

project(external-magnum)
include(ExternalProject)

list(APPEND CMAKE_ARGS
    "-DCMAKE_PREFIX_PATH:PATH=${CMAKE_PREFIX_PATH}"
    "-DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_INSTALL_PREFIX}"
    "-DCMAKE_TOOLCHAIN_FILE:PATH=${CMAKE_TOOLCHAIN_FILE}"
    "-DCMAKE_POSITION_INDEPENDENT_CODE=ON"
    "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
    "-DBUILD_SHARED_LIBS=OFF"
    "-DMAGNUM_WITH_SDL2APPLICATION=ON"
    )

if(IOS)
    list(APPEND CMAKE_ARGS
        "-DPLATFORM=${PLATFORM}"
        "-DDEPLOYMENT_TARGET=${DEPLOYMENT_TARGET}"
        "-DENABLE_STRICT_TRY_COMPILE=${ENABLE_STRICT_TRY_COMPILE}"
        )
endif()

message(STATUS "Preparing external project \"magnum\" with args:")
foreach(CMAKE_ARG ${CMAKE_ARGS})
    message(STATUS "-- ${CMAKE_ARG}")
endforeach()

ExternalProject_Add(
    magnum
    GIT_REPOSITORY https://github.com/mosra/magnum.git
    PREFIX magnum 
    SOURCE_DIR ${CMAKE_BINARY_DIR}/_deps/src/magnum # Clone directory
    BINARY_DIR ${CMAKE_BINARY_DIR}/_deps/build/magnum # Build directory 
    CMAKE_ARGS "${CMAKE_ARGS}"
    )

# add magnum dependencies
add_subdirectory(corrade)
add_dependencies(magnum
    corrade
)

# 
