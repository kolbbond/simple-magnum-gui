# External dependencies for simple-magnum-gui
# Build order matters: corrade -> magnum -> magnum-integration

# Corrade options
set(CORRADE_BUILD_TESTS OFF CACHE BOOL "" FORCE)

# Magnum options
set(MAGNUM_WITH_SDL2APPLICATION ON CACHE BOOL "" FORCE)
set(MAGNUM_WITH_GL ON CACHE BOOL "" FORCE)
set(MAGNUM_WITH_MESHTOOLS ON CACHE BOOL "" FORCE)
set(MAGNUM_WITH_PRIMITIVES ON CACHE BOOL "" FORCE)
set(MAGNUM_WITH_SHADERS ON CACHE BOOL "" FORCE)
set(MAGNUM_WITH_TRADE ON CACHE BOOL "" FORCE)
set(MAGNUM_WITH_TEXT ON CACHE BOOL "" FORCE)
set(MAGNUM_WITH_MAGNUMFONT ON CACHE BOOL "" FORCE)
set(MAGNUM_BUILD_TESTS OFF CACHE BOOL "" FORCE)

# Magnum Integration options (imgui is a submodule inside magnum-integration)
set(MAGNUM_WITH_IMGUI ON CACHE BOOL "" FORCE)
set(MAGNUM_BUILD_TESTS OFF CACHE BOOL "" FORCE)

# Build in order
add_subdirectory(corrade)
add_subdirectory(magnum)
add_subdirectory(magnum-integration)

# ImPlot/ImPlot3D don't have CMakeLists - sources are included directly by smg
# Just expose the directories
set(SMG_IMPLOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/implot" CACHE PATH "" FORCE)
set(SMG_IMPLOT3D_DIR "${CMAKE_CURRENT_SOURCE_DIR}/implot3d" CACHE PATH "" FORCE)
