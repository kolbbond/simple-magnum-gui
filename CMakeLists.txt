# build imgui with implot
cmake_minimum_required(VERSION 3.10)

project(guild)

# switchboard
option(USE_SDL ON)

# find packages
# which backend?

# set sources
# imgui and implot

add_library(${PROJECT_NAME} INTERFACE)
#target_include_directories(${PROJECT_NAME} PRIVATE "include")


# add tests?
# we can probably steal from the examples?
# add test subdir
#enable_testing()
#add_subdirectory(tests)
