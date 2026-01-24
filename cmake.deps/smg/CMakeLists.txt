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

message(WARNING "prefix path: ${CMAKE_PREFIX_PATH}")
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

# Install built SMG dependencies (Corrade, Magnum, MagnumPlugins, MagnumIntegration)
# to the final install prefix so consuming projects can find them via find_package()
# DEPS_INSTALL_DIR is inherited from parent cmake.deps/CMakeLists.txt
if(NOT DEFINED DEPS_INSTALL_DIR)
    # Fallback if not defined (shouldn't happen when built via cmake.deps)
    set(DEPS_INSTALL_DIR "${CMAKE_BINARY_DIR}/../usr")
endif()
message(STATUS "SMG deps will install from: ${DEPS_INSTALL_DIR}")

install(DIRECTORY ${DEPS_INSTALL_DIR}/bin/
    DESTINATION bin
    USE_SOURCE_PERMISSIONS
    OPTIONAL
)
install(DIRECTORY ${DEPS_INSTALL_DIR}/lib/
    DESTINATION lib
    USE_SOURCE_PERMISSIONS
    OPTIONAL
)
install(DIRECTORY ${DEPS_INSTALL_DIR}/include/
    DESTINATION include
    USE_SOURCE_PERMISSIONS
    OPTIONAL
)
install(DIRECTORY ${DEPS_INSTALL_DIR}/share/
    DESTINATION share
    USE_SOURCE_PERMISSIONS
    OPTIONAL
)

