## Project setup
cmake_minimum_required(VERSION 3.10)
project(gui_console VERSION 0.1.0 LANGUAGES C CXX)

message(STATUS "starting cmake generation: " ${PROJECT_NAME})

# switchboard
option(USE_CMAKEHELPERS "for debug" ON)

if(USE_CMAKEHELPERS)
    list(APPEND CMAKE_MODULE_PATH "$ENV{CMAKEHELPERS_DIR}")
    include(PrintHelpers)
endif(USE_CMAKEHELPERS)

# check system
message(STATUS "check system architecture")
if(WIN32)
    message(STATUS "this is a windows machine")
    message(ERROR "windows not implemented")

endif(WIN32)
if(UNIX)
    message(STATUS "this is a linux machine")
endif(UNIX)

# ensure out of core build
if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(FATAL_ERROR "In-source builds not allowed.\nPlease make a new directory (called a build directory) and run CMake from there.\n")
endif()

message(STATUS "find packages")


# find libraries
find_package(guild REQUIRED)

##################################################
# Create libraries

# add source list
# ls *.cpp >> CMakeLists.txt
set(source_list
    main.cpp
)

add_executable(${PROJECT_NAME} ${source_list})


# enable warnings
message(STATUS "compiler id: " ${CXX_COMPILER_ID})
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
        -Wpedantic -Wall -Wextra>
    )

# link to library
#link_directories(${LIB_DIR})
#target_link_directories(${PROJECT_NAME} PUBLIC ${LIB_DIR})
target_link_libraries(${PROJECT_NAME} PUBLIC
    guild::guild
 )

