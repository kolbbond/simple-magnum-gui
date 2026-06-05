# External dependencies for simple-magnum-gui
# Build order matters: corrade -> magnum -> magnum-integration

# Corrade options
set(CORRADE_BUILD_TESTS OFF CACHE BOOL "" FORCE)

# Emscripten-specific configuration
if(EMSCRIPTEN)
    # Use EmscriptenApplication for WASM builds
    set(MAGNUM_WITH_EMSCRIPTENAPPLICATION ON CACHE BOOL "" FORCE)
    set(MAGNUM_WITH_SDL2APPLICATION OFF CACHE BOOL "" FORCE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s USE_WEBGL2=1 -s ALLOW_MEMORY_GROWTH=1")
else()
    # Desktop: use SDL2Application
    set(MAGNUM_WITH_EMSCRIPTENAPPLICATION OFF CACHE BOOL "" FORCE)
    set(MAGNUM_WITH_SDL2APPLICATION ON CACHE BOOL "" FORCE)
endif()

# Magnum options (common)
set(MAGNUM_WITH_GL ON CACHE BOOL "" FORCE)
set(MAGNUM_WITH_MESHTOOLS ON CACHE BOOL "" FORCE)
set(MAGNUM_WITH_PRIMITIVES ON CACHE BOOL "" FORCE)
set(MAGNUM_WITH_SHADERS ON CACHE BOOL "" FORCE)
set(MAGNUM_WITH_TRADE ON CACHE BOOL "" FORCE)
set(MAGNUM_WITH_TEXT ON CACHE BOOL "" FORCE)
set(MAGNUM_WITH_MAGNUMFONT ON CACHE BOOL "" FORCE)
set(MAGNUM_WITH_SCENEGRAPH ${ENABLE_SCENEGRAPH} CACHE BOOL "" FORCE)
set(MAGNUM_BUILD_TESTS OFF CACHE BOOL "" FORCE)

# Disable DebugTools on Windows (has DLL export issues with template math classes)
if(WIN32)
    set(MAGNUM_WITH_DEBUGTOOLS OFF CACHE BOOL "" FORCE)
endif()

# Magnum Plugins options
set(MAGNUM_WITH_ANYIMAGEIMPORTER ON CACHE BOOL "" FORCE)
set(MAGNUM_WITH_STBIMAGEIMPORTER ON CACHE BOOL "" FORCE)
set(MAGNUM_WITH_ANYSCENEIMPORTER ON CACHE BOOL "" FORCE)
set(MAGNUM_WITH_OBJIMPORTER ON CACHE BOOL "" FORCE)
set(MAGNUM_BUILD_PLUGINS_STATIC OFF CACHE BOOL "" FORCE)

# Magnum Integration options (imgui is a submodule inside magnum-integration)
set(MAGNUM_WITH_IMGUIINTEGRATION ON CACHE BOOL "" FORCE)

# Build in order
add_subdirectory(corrade)
add_subdirectory(magnum)
add_subdirectory(magnum-plugins)
add_subdirectory(magnum-integration)

# ImPlot/ImPlot3D don't have CMakeLists - sources are included directly by smg
# Just expose the directories
set(SMG_IMPLOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/implot" CACHE PATH "" FORCE)
set(SMG_IMPLOT3D_DIR "${CMAKE_CURRENT_SOURCE_DIR}/implot3d" CACHE PATH "" FORCE)
