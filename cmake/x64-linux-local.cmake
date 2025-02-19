# custom toolchain file for x64-linux-local
set(TRIPLET_NAME x64-linux-local)
message(STATUS "vcpkg build with triplet: ${TRIPLET_NAME}")
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)
#set(VCPKG_BUILD_TYPE release)

#set(VCPKG_CXX_FLAGS "${VCPKG_CXX_FLAGS} -fsanitize=address")

# set environment variables
set(VCPKG_ENV_PASSTHROUGH HOME)
set(USER_DIR $ENV{HOME})
message("User Folder: ${CMAKE_CURRENT_LIST_DIR}")
set(ENV{BLA_VENDOR} OpenBLAS)
set(ENV{LD_LIBRARY_PATH} ${USER_DIR}/.local/vcpkg/x64-linux-local/lib)

# environment variables set
message(STATUS "Environment variables set:")
message(STATUS "  USER_DIR: ${USER_DIR}")
message(STATUS "  BLA_VENDOR: ${BLA_VENDOR}")
message(STATUS "  LD_LIBRARY_PATH: ${LD_LIBRARY_PATH}")

