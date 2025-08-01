# magnumplugins options

# create a prefix path for magnumplugins to use
list(APPEND _magnumplugins_prefix_paths "${CMAKE_PREFIX_PATH}") # propagate prefix paths
list(JOIN _magnumplugins_prefix_paths ";" _magnumplugins_prefix_path_string)
string(REPLACE ";" "|" _magnumplugins_prefix_path_fixed "${_magnumplugins_prefix_path_string}")

# from bash script
#    -DMAGNUM_WITH_DRWAVIMPORTER=ON \
#    -DMAGNUM_WITH_JPEGIMPORTER=ON \
#    -DMAGNUM_WITH_PNGIMPORTER=ON \
#    -DMAGNUM_WITH_FREETYPEFONT=ON \
#    -DMAGNUM_WITH_PNGIMAGECONVERTER=ON;

# set cmake arguments
set(magnumplugins_CMAKE_ARGS
    -D CMAKE_PREFIX_PATH=${_magnumplugins_prefix_path_fixed}
    -D MAGNUM_WITH_DRWAVIMPORTER=ON 
    -D MAGNUM_WITH_JPEGIMPORTER=ON 
    -D MAGNUM_WITH_PNGIMPORTER=ON 
    -D MAGNUM_WITH_FREETYPEFONT=ON 
    -D MAGNUM_WITH_PNGIMAGECONVERTER=ON
  )

  #list(APPEND magnumplugins_CMAKE_ARGS -D WITH_LUA_ENGINE=${LUA_ENGINE})

  # pull repo

get_externalproject_options(magnumplugins ${DEPS_IGNORE_SHA})
ExternalProject_Add(magnumplugins
    #DEPENDS lua_compat53
  DOWNLOAD_DIR ${DEPS_DOWNLOAD_DIR}/magnumplugins
  SOURCE_DIR ${DEPS_BUILD_DIR}/src/magnumplugins
  CMAKE_ARGS ${DEPS_CMAKE_ARGS} ${magnumplugins_CMAKE_ARGS}
  LIST_SEPARATOR |
  ${EXTERNALPROJECT_OPTIONS})

add_dependencies(magnumplugins 
    corrade
    magnum
)


