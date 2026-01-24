# simple-magnum-gui - ImGui with Magnum
cmake_minimum_required(VERSION 3.16)

project(
  simple-magnum-gui
  VERSION 0.1
  LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message(STATUS "Configuring ${PROJECT_NAME}")

# Options
option(ENABLE_TESTING "build unit/system tests" ON)
option(ENABLE_EXAMPLES "build examples" ON)
option(ENABLE_IMPLOT3D "build with ImPlot3D" ON)

# Install only makes sense when building standalone (not as subdirectory)
# because we can't export targets that depend on non-exported subdirectory targets
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    option(CREATE_INSTALL "install into system dir" ON)
else()
    set(CREATE_INSTALL OFF)
endif()

# Module path
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

# Export compile commands for clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_COLOR_DIAGNOSTICS ON)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# System-specific settings
if(WIN32)
    message(STATUS "Building on Windows")
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
elseif(UNIX)
    message(STATUS "Building on Linux/Unix")
    list(INSERT CMAKE_PREFIX_PATH 0 "/usr/lib/x86_64-linux-gnu/")
endif()

# Prevent in-source builds
if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(FATAL_ERROR "In-source builds not allowed. Use a separate build directory.")
endif()

# Output paths
if(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin CACHE PATH "" FORCE)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib CACHE PATH "" FORCE)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib CACHE PATH "" FORCE)
endif()

# Build external dependencies (Corrade, Magnum, Magnum-Integration)
add_subdirectory(external)

# ImPlot paths (from external/)
set(SMG_IMPLOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/implot")
set(SMG_IMPLOT3D_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/implot3d")

# Sources
set(smg_sources
    src/GuiBase.cpp
    src/DrawCallback.cpp
    src/log.cpp
    src/Statics.cpp

    # ImPlot
    ${SMG_IMPLOT_DIR}/implot.cpp
    ${SMG_IMPLOT_DIR}/implot_demo.cpp
    ${SMG_IMPLOT_DIR}/implot_items.cpp
)

if(ENABLE_IMPLOT3D)
    list(APPEND smg_sources
        ${SMG_IMPLOT3D_DIR}/implot3d.cpp
        ${SMG_IMPLOT3D_DIR}/implot3d_items.cpp
        ${SMG_IMPLOT3D_DIR}/implot3d_demo.cpp
        ${SMG_IMPLOT3D_DIR}/implot3d_meshes.cpp
    )
endif()

# Corrade resources
corrade_add_resource(smg_RESOURCE_fonts "assets/resource_fonts.conf")
corrade_add_resource(smg_RESOURCE_images "assets/resource_images.conf")

# Library
add_library(smg SHARED
    ${smg_sources}
    ${smg_RESOURCE_fonts}
    ${smg_RESOURCE_images}
)

add_library(smg::smg ALIAS smg)

# Link dependencies
target_link_libraries(smg PUBLIC
    Corrade::Utility
    Magnum::Sdl2Application
    Magnum::Magnum
    Magnum::GL
    Magnum::MeshTools
    Magnum::Primitives
    Magnum::Shaders
    Magnum::Trade
    Magnum::Text
    MagnumIntegration::ImGui
)

if(UNIX AND NOT APPLE)
    find_package(OpenGL REQUIRED)
    target_link_libraries(smg PUBLIC OpenGL::GLX)
endif()

# Include directories
target_include_directories(smg
    PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${SMG_IMPLOT_DIR}>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(ENABLE_IMPLOT3D)
    target_include_directories(smg PUBLIC
        $<BUILD_INTERFACE:${SMG_IMPLOT3D_DIR}>
    )
endif()

# Compiler warnings
target_compile_options(smg PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
    -Wall -Wextra -Wpedantic>
)

# Configure headers
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/smg.hh.in
    ${CMAKE_BINARY_DIR}/include/smg.hh
)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.hh.in
    ${CMAKE_BINARY_DIR}/include/version.hh
)

# Tests
if(ENABLE_TESTING)
    enable_testing()
    add_subdirectory(test)
endif()

# Examples
if(ENABLE_EXAMPLES)
    add_subdirectory(examples)
endif()

# Installation
if(CREATE_INSTALL)
    include(GNUInstallDirs)
    set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/smg)

    install(TARGETS smg
        EXPORT smg-targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/smg
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/smg
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    set_target_properties(smg PROPERTIES EXPORT_NAME smg)

    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/smg)
    install(FILES
        ${CMAKE_BINARY_DIR}/include/version.hh
        ${CMAKE_BINARY_DIR}/include/smg.hh
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/smg
    )
    install(FILES ${SMG_IMPLOT_DIR}/implot.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/smg
    )

    if(ENABLE_IMPLOT3D)
        install(FILES ${SMG_IMPLOT3D_DIR}/implot3d.h
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/smg
        )
    endif()

    install(EXPORT smg-targets
        FILE smgTargets.cmake
        NAMESPACE smg::
        DESTINATION ${INSTALL_CONFIGDIR}
    )

    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/smgConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY AnyNewerVersion
    )

    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/smgConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/smgConfig.cmake
        INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
        PATH_VARS CMAKE_INSTALL_INCLUDEDIR CMAKE_INSTALL_LIBDIR
    )

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/smgConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/smgConfigVersion.cmake
        DESTINATION ${INSTALL_CONFIGDIR}
    )

    export(EXPORT smg-targets
        FILE ${CMAKE_CURRENT_BINARY_DIR}/smgTargets.cmake
    )
endif()

# Summary
message("\n"
    "Build Summary\n"
    "-------------\n"
    "* Install Directory : ${CMAKE_INSTALL_PREFIX}\n"
    "* Build Type        : ${CMAKE_BUILD_TYPE}\n"
    "* Tests             : ${ENABLE_TESTING}\n"
    "* Examples          : ${ENABLE_EXAMPLES}\n"
    "* ImPlot3D          : ${ENABLE_IMPLOT3D}\n"
)

# Windows: copy SDL2 DLL
if(WIN32)
    add_custom_command(TARGET smg POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:SDL2::SDL2>" "$<TARGET_FILE_DIR:smg>"
        VERBATIM
    )
endif()
