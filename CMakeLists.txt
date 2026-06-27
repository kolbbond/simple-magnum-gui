# simple-magnum-gui - ImGui with Magnum
cmake_minimum_required(VERSION 3.16)

project(
  simple-magnum-gui
  VERSION 0.1.0
  LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message(STATUS "Configuring ${PROJECT_NAME}")

# Emscripten/WASM detection
if(EMSCRIPTEN)
    message(STATUS "Building for Emscripten/WASM")
    set(SMG_PLATFORM_WASM ON)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
else()
    set(SMG_PLATFORM_WASM OFF)
endif()

# Options
option(ENABLE_TESTING "build unit/system tests" ON)
option(ENABLE_EXAMPLES "build examples" ON)
option(ENABLE_IMPLOT3D "build with ImPlot3D" ON)
option(ENABLE_SCENEGRAPH "build with Magnum SceneGraph for 3D scene management" OFF)


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

# Option to use system-installed dependencies instead of building from source
option(USE_SYSTEM_MAGNUM "Use system-installed Magnum/Corrade instead of building from source" OFF)

if(USE_SYSTEM_MAGNUM)
    # Find system packages
    find_package(Corrade REQUIRED Utility PluginManager TestSuite)
    find_package(Magnum REQUIRED
        GL
        MeshTools
        Primitives
        Shaders
        Trade
        Text
        Sdl2Application)
    if(ENABLE_SCENEGRAPH)
        find_package(Magnum REQUIRED SceneGraph)
    endif()
    find_package(MagnumIntegration REQUIRED ImGui)

    # ImPlot paths (still from our external/ since they're header-only style)
    set(SMG_IMPLOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/implot")
    set(SMG_IMPLOT3D_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/implot3d")
else()
    # Build external dependencies (Corrade, Magnum, Magnum-Integration)
    # These have their own install rules and will be installed alongside smg
    # Keep plugin discovery working in build trees on multi-config generators.
    set(MAGNUM_PLUGINS_DEBUG_DIR "${CMAKE_BINARY_DIR}/bin/magnum-d" CACHE PATH "" FORCE)
    set(MAGNUM_PLUGINS_RELEASE_DIR "${CMAKE_BINARY_DIR}/bin/magnum" CACHE PATH "" FORCE)
    add_subdirectory(external)
endif()

# ImPlot paths (from external/ - set if not already set by external/CMakeLists.txt)
if(NOT DEFINED SMG_IMPLOT_DIR)
    set(SMG_IMPLOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/implot")
endif()
if(NOT DEFINED SMG_IMPLOT3D_DIR)
    set(SMG_IMPLOT3D_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/implot3d")
endif()

# Sources
set(smg_sources
    src/GuiBase.cpp
    src/DrawCallback.cpp
    src/log.cpp
    src/Statics.cpp
    src/SceneTypes.cpp
    src/Camera.cpp
    src/Mesh.cpp
    src/Primitives.cpp
    src/ScenePanel.cpp
    src/IsoGrid.cpp
    src/SpriteSheet.cpp

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

# Bloom post-process (desktop only -- shaders require GL330)
if(NOT SMG_PLATFORM_WASM)
    set(SMG_BLOOM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/bloom)
    list(APPEND smg_sources
        ${SMG_BLOOM_DIR}/src/bloomrenderer.cpp
        ${SMG_BLOOM_DIR}/src/bloomfbo.cpp
        ${SMG_BLOOM_DIR}/src/bloomdownsample.cpp
        ${SMG_BLOOM_DIR}/src/bloomupsample.cpp
        ${SMG_BLOOM_DIR}/src/bloomfinal.cpp
    )
    corrade_add_resource(smg_RESOURCE_bloom "${SMG_BLOOM_DIR}/resources.conf")
endif()

# ImGuiFileDialog (all platforms -- pure Dear ImGui, no GL dependency)
set(SMG_IMGUIFILEDIALOG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/imguifiledialog)
list(APPEND smg_sources
    src/FileDialog.cpp
    ${SMG_IMGUIFILEDIALOG_DIR}/ImGuiFileDialog.cpp
)

# Corrade resources
corrade_add_resource(smg_RESOURCE_fonts "assets/resource_fonts.conf")
corrade_add_resource(smg_RESOURCE_images "assets/resource_images.conf")

# Library (static for WASM, shared otherwise)
if(SMG_PLATFORM_WASM)
    add_library(smg STATIC
        ${smg_sources}
        ${smg_RESOURCE_fonts}
        ${smg_RESOURCE_images}
    )
else()
    add_library(smg SHARED
        ${smg_sources}
        ${smg_RESOURCE_fonts}
        ${smg_RESOURCE_images}
        ${smg_RESOURCE_bloom}
    )
endif()

add_library(smg::smg ALIAS smg)

# Link dependencies
target_link_libraries(smg PUBLIC
    Corrade::Utility
    Magnum::Magnum
    Magnum::GL
    Magnum::MeshTools
    Magnum::Primitives
    Magnum::Shaders
    Magnum::Trade
    Magnum::Text
    MagnumIntegration::ImGui
)

if(ENABLE_SCENEGRAPH)
    target_link_libraries(smg PUBLIC Magnum::SceneGraph)
endif()

# Platform-specific application library
if(SMG_PLATFORM_WASM)
    target_link_libraries(smg PUBLIC Magnum::EmscriptenApplication)
else()
    target_link_libraries(smg PUBLIC Magnum::Sdl2Application)
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

# Bloom: private include (ScenePanel forward-declares the renderer); public
# define so consumers and the library agree on ScenePanel's layout.
if(NOT SMG_PLATFORM_WASM)
    target_include_directories(smg PRIVATE $<BUILD_INTERFACE:${SMG_BLOOM_DIR}/include>)
    target_compile_definitions(smg PUBLIC SMG_WITH_BLOOM)
endif()

# ImGuiFileDialog headers are private -- smg::FileDialog is the public surface
target_include_directories(smg PRIVATE $<BUILD_INTERFACE:${SMG_IMGUIFILEDIALOG_DIR}>)

# Compiler warnings
target_compile_options(smg PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
    -Wall -Wextra -Wpedantic>
)

# WASM-specific compile definitions
if(SMG_PLATFORM_WASM)
    target_compile_definitions(smg PUBLIC SMG_PLATFORM_WASM)
endif()

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
include(GNUInstallDirs)
set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/smg)

# Install the library target
# When building dependencies from source, we can't export targets that reference
# non-exported dependency targets. Only create export set with system packages.
if(USE_SYSTEM_MAGNUM)
    install(TARGETS smg
        EXPORT smg-targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/smg
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/smg
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    set_target_properties(smg PROPERTIES EXPORT_NAME smg)
else()
    install(TARGETS smg
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/smg
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/smg
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

# Install headers (always)
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

# Generate and install config files (always)
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

# Export targets
if(USE_SYSTEM_MAGNUM)
    # Use CMake's export mechanism when deps are imported targets
    install(EXPORT smg-targets
        FILE smgTargets.cmake
        NAMESPACE smg::
        DESTINATION ${INSTALL_CONFIGDIR}
    )

    export(EXPORT smg-targets
        FILE ${CMAKE_CURRENT_BINARY_DIR}/smgTargets.cmake
    )
else()
    # Use manual template when building deps from source
    # (CMake export doesn't work with add_subdirectory targets)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/smgTargets.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/smgTargets.cmake
        @ONLY
    )

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/smgTargets.cmake
        DESTINATION ${INSTALL_CONFIGDIR}
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
if(WIN32 AND TARGET SDL2::SDL2)
    add_custom_command(TARGET smg POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:SDL2::SDL2>" "$<TARGET_FILE_DIR:smg>"
        VERBATIM
    )
endif()
