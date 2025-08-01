project(smg-build)
message(STATUS "Building smg")

# Point CMake at any custom modules we may ship
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" "${PROJECT_SOURCE_DIR}/../cmake")
#include(ExternalProject)

#message(WARNING "prefix path: ${CMAKE_PREFIX_PATH}")

# prefix and cmake args
list(APPEND _smg_prefix_paths "${CMAKE_PREFIX_PATH}") # propagate prefix paths
list(JOIN _smg_prefix_paths ";" _smg_prefix_path_string)
string(REPLACE ";" "|" _smg_prefix_path_fixed "${_smg_prefix_path_string}")
set(smg_CMAKE_ARGS
    -D CMAKE_PREFIX_PATH=${_smg_prefix_path_fixed}
  )

#message(WARNING "prefix path: ${CMAKE_PREFIX_PATH}")
#message(FATAL_ERROR "prefix path: ${CMAKE_PREFIX_PATH}")

# build rat dependencies
option(USE_BUNDLED_MAGNUM "use bundled magnum here" ${USE_BUNDLED})

# build superlu
if(USE_BUNDLED_MAGNUM)
    include(BuildCorrade)
    include(BuildMagnum)
    include(BuildMagnumPlugins)
    include(BuildMagnumIntegration)
endif()

