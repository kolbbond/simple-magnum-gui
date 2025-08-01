# magnum options
message(STATUS "Building Magnum")

# create a prefix path for magnum to use
list(APPEND _magnum_prefix_paths "${CMAKE_PREFIX_PATH}") # propagate prefix paths
list(JOIN _magnum_prefix_paths ";" _magnum_prefix_path_string)
string(REPLACE ";" "|" _magnum_prefix_path_fixed "${_magnum_prefix_path_string}")


#message(WARNING ${_magnum_prefix_path_fixed})

# from bash script
#    -DMAGNUM_WITH_SDL2APPLICATION=ON \
#    -DMAGNUM_WITH_ANYIMAGEIMPORTER=ON \
#    -DMAGNUM_WITH_ANYIMAGECONVERTER=ON \
#    -DMAGNUM_WITH_ANYSCENECONVERTER=ON \
#    -DMAGNUM_WITH_ANYSCENEIMPORTER=ON \
#    -DMAGNUM_WITH_OBJIMPORTER=ON \
#    -DMAGNUM_WITH_AUDIO=ON
#
set(magnum_CMAKE_ARGS
    -D CMAKE_PREFIX_PATH=${_magnum_prefix_path_fixed}
    -D MAGNUM_WITH_SDL2APPLICATION=ON 
    -D MAGNUM_WITH_ANYIMAGEIMPORTER=ON 
    -D MAGNUM_WITH_ANYIMAGECONVERTER=ON 
    -D MAGNUM_WITH_ANYSCENECONVERTER=ON 
    -D MAGNUM_WITH_ANYSCENEIMPORTER=ON 
    -D MAGNUM_WITH_OBJIMPORTER=ON 
    -D MAGNUM_WITH_MAGNUMFONT=ON
    -D MAGNUM_WITH_AUDIO=OFF
  )

  #list(APPEND magnum_CMAKE_ARGS -D WITH_LUA_ENGINE=${LUA_ENGINE})

  # pull repo

get_externalproject_options(magnum ${DEPS_IGNORE_SHA})
#message(WARNING ${EXTERNALPROJECT_OPTIONS})

ExternalProject_Add(magnum
    #DEPENDS lua_compat53
  DOWNLOAD_DIR ${DEPS_DOWNLOAD_DIR}/magnum
  SOURCE_DIR ${DEPS_BUILD_DIR}/src/magnum
  CMAKE_ARGS ${DEPS_CMAKE_ARGS} ${magnum_CMAKE_ARGS}
  LIST_SEPARATOR |
  ${EXTERNALPROJECT_OPTIONS})

add_dependencies(magnum corrade)


