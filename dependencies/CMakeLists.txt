# use External Project to build dependencies
cmake_minimum_required(VERSION 3.10.2)

set(CMAKE_INSTALL_PREFIX "~/.local")

project(dependencies)

# build magnum-integration
add_subdirectory(magnum-integration)
