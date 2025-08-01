# armadillo options
# from bash script
# cmake -B build -DCMAKE_INSTALL_PREFIX=~/.local -DOPENBLAS_PROVIDES_LAPACK=true -DARMA_USE_SUPERLU=true;
set(armadillo_CMAKE_ARGS
    -D STATIC_LIB=OFF
    -D OPENBLAS_PROVIDES_LAPACK=true
    -D ARMA_USE_SUPERLU=true
  )

  #list(APPEND armadillo_CMAKE_ARGS -D WITH_LUA_ENGINE=${LUA_ENGINE})

  # pull repo

get_externalproject_options(armadillo ${DEPS_IGNORE_SHA})
ExternalProject_Add(armadillo
    #DEPENDS lua_compat53
  DOWNLOAD_DIR ${DEPS_DOWNLOAD_DIR}/armadillo
  SOURCE_DIR ${DEPS_BUILD_DIR}/src/armadillo
  CMAKE_ARGS ${DEPS_CMAKE_ARGS} ${armadillo_CMAKE_ARGS}
  ${EXTERNALPROJECT_OPTIONS})


