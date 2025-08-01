# corrade options
message(STATUS "Building Corrade")

# from bash script
set(corrade_CMAKE_ARGS
    -D CORRADE_BUILD_TESTS=OFF
    -D CORRADE_BUILD_SHARED=ON
  )

message(STATUS "corrade url: ${CORRADE_URL}")

  # pull repo
get_externalproject_options(corrade ${DEPS_IGNORE_SHA})
ExternalProject_Add(corrade
  DOWNLOAD_DIR ${DEPS_DOWNLOAD_DIR}/corrade
  SOURCE_DIR ${DEPS_BUILD_DIR}/src/corrade
  CMAKE_ARGS ${DEPS_CMAKE_ARGS} ${corrade_CMAKE_ARGS}
  BUILD_ALWAYS TRUE
  ${EXTERNALPROJECT_OPTIONS})

#add_dependencies(corrade)


