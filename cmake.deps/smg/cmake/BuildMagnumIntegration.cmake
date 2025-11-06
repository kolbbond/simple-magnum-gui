# magnumintegration options
message(STATUS "Building MagnumIntegration")

# create a prefix path for magnumintegration to use
list(APPEND _magnumintegration_prefix_paths "${CMAKE_PREFIX_PATH}") # propagate prefix paths
list(JOIN _magnumintegration_prefix_paths ";" _magnumintegration_prefix_path_string)
string(REPLACE ";" "|" _magnumintegration_prefix_path_fixed "${_magnumintegration_prefix_path_string}")

#message(WARNING ${_magnumintegration_prefix_path_fixed})

# from bash script
#   -DMAGNUM_WITH_IMGUI=ON;
set(magnumintegration_CMAKE_ARGS
    -D CMAKE_PREFIX_PATH=${_magnumintegration_prefix_path_fixed}
    -D MAGNUM_WITH_IMGUI=ON
  )

set(imgui_path "${DEPS_BUILD_DIR}/src/magnumintegration/src/MagnumExternal/ImGui")

set(my_configure_command "${CMAKE_COMMAND} -S ${DEPS_BUILD_DIR}/src/magnumintegration -B ${DEPS_BUILD_DIR}/build/magnumintegration ${DEPS_CMAKE_ARGS} ${magnumintegration_CMAKE_ARGS}")

get_externalproject_options(magnumintegration ${DEPS_IGNORE_SHA})
ExternalProject_Add(magnumintegration
  DOWNLOAD_DIR ${DEPS_DOWNLOAD_DIR}/magnumintegration
  SOURCE_DIR ${DEPS_BUILD_DIR}/src/magnumintegration

  #  UPDATE_COMMAND
  #    ${CMAKE_COMMAND} -E echo "Cloning imgui..." &&
  #    ${CMAKE_COMMAND} -E remove_directory ${imgui_path} &&
  #    ${CMAKE_COMMAND} -E make_directory ${imgui_path} &&
  #    git clone --branch v1.91.9 --depth 1 https://github.com/ocornut/imgui.git ${imgui_path}
    UPDATE_COMMAND
    ${CMAKE_COMMAND} -Dimgui_path=${imgui_path} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/update_imgui.cmake


    CMAKE_ARGS ${DEPS_CMAKE_ARGS} ${magnumintegration_CMAKE_ARGS}
    #CMAKE_ARGS ${magnumintegration_CMAKE_ARGS}

  LIST_SEPARATOR |

  ${EXTERNALPROJECT_OPTIONS}

)

#add_dependencies(magnumintegration-configure
add_dependencies(magnumintegration
    corrade
    magnum
    magnumplugins
)

# fake dependency to force externalproject to build
add_custom_target(build_magnumintegration ALL)
add_dependencies(build_magnumintegration magnumintegration)
#message(FATAL_ERROR "end of magnumintegration")


