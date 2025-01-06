cmake_minimum_required(VERSION 3.1)

project(external-magnum-integration)
include(ExternalProject)

list(APPEND CMAKE_ARGS
    "-DCMAKE_PREFIX_PATH:PATH=${CMAKE_PREFIX_PATH}"
    "-DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_INSTALL_PREFIX}"
    "-DCMAKE_TOOLCHAIN_FILE:PATH=${CMAKE_TOOLCHAIN_FILE}"
    "-DCMAKE_POSITION_INDEPENDENT_CODE=ON"
    "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
    "-DBUILD_SHARED_LIBS=OFF"
    "-DMAGNUM_WITH_IMGUI=ON"
    )

if(IOS)
    list(APPEND CMAKE_ARGS
        "-DPLATFORM=${PLATFORM}"
        "-DDEPLOYMENT_TARGET=${DEPLOYMENT_TARGET}"
        "-DENABLE_STRICT_TRY_COMPILE=${ENABLE_STRICT_TRY_COMPILE}"
        )
endif()

message(STATUS "Preparing external project \"magnum-integration\" with args:")
foreach(CMAKE_ARG ${CMAKE_ARGS})
    message(STATUS "-- ${CMAKE_ARG}")
endforeach()


# download magnum-integration and imgui
include(FetchContent)

FetchContent_Declare(magnum-integration
    GIT_REPOSITORY https://github.com/mosra/magnum-integration.git
    GIT_TAG master
    SOURCE_DIR "${CMAKE_BINARY_DIR}/_deps/src/magnum-integration/"
)

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG master
    SOURCE_DIR "${CMAKE_BINARY_DIR}/_deps/src/magnum-integration/src/MagnumExternal/ImGui"
)
set(FETCHCONTENT_BASE_DIR ${imgui_DownloadDir})

# Ensure the content is downloaded and available
FetchContent_MakeAvailable(magnum-integration)
FetchContent_MakeAvailable(imgui)

ExternalProject_Add( magnum-integration
    SOURCE_DIR ${CMAKE_BINARY_DIR}/_deps/src/magnum-integration# Clone directory
    PREFIX magnum-integration
    BINARY_DIR ${CMAKE_BINARY_DIR}/_deps/build/magnum-integration # Build directory 
    CMAKE_ARGS "${CMAKE_ARGS}"
    )

# add magnum-integration dependencies
add_subdirectory(magnum)
add_dependencies(magnum-integration
    magnum
)
