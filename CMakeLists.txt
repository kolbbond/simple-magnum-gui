# build imgui with implot
cmake_minimum_required(VERSION 3.10)

project(
  guild
  VERSION 0.1
  LANGUAGES C CXX)

message(STATUS "starting cmake generation: " ${PROJECT_NAME})

# switchboard
option(ENABLE_TESTING "build unit/system tests" ON)
option(USE_SDL "use SDL for window" ON)
option(USE_OPENGL "use OpenGL for rendering" ON)
option(USE_CMAKEHELPERS "use cmakehelpers for debug" OFF)
option(CREATE_INSTALL "install into general system dir" OFF)
option(FLAG_DEBUG "include debug scripts" OFF)

# if cmake helpers - for debug
if(USE_CMAKEHELPERS)
  list(APPEND CMAKE_MODULE_PATH "$ENV{CMAKEHELPERS_DIR}")
  include(PrintVariables)
endif(USE_CMAKEHELPERS)

# system detection
if(WIN32)
  message(STATUS "this is a linux machine")
endif(WIN32)
if(UNIX)
  message(STATUS "this is a linux machine")
endif(UNIX)

# ensure out of core build
if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(
    FATAL_ERROR
      "In-source builds not allowed (for your own safety).\nPlease make a new directory (called a build directory) and run CMake from there.\n"
  )
endif()

# find packages
message(STATUS "find packages")
if(USE_OPENGL)
  set(OpenGL_GL_PREFERENCE "GLVND")
  find_package(OpenGL REQUIRED)
endif(USE_OPENGL)
if(USE_SDL)
  find_package(SDL2 REQUIRED)
endif(USE_SDL)

# which backend?

# set sources imgui and implot
set(source_list
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_sdl2.cpp
    imgui/backends/imgui_impl_opengl3.cpp
    imgui/imgui_demo.cpp
    implot/implot.cpp
    implot/implot_items.cpp
    implot/implot_demo.cpp
)

# make our library (shared or static?)
add_library(${PROJECT_NAME} SHARED ${source_list})

# include directories we are making an interface for projects that link to THIS
# library (includes all dependencies in our lib)
target_include_directories(
  ${PROJECT_NAME}
  PUBLIC $<INSTALL_INTERFACE:include>
         $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/imgui>
         $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends>
         $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/implot>
         $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
)

# link to library
target_link_libraries(${PROJECT_NAME} PUBLIC 
    SDL2::SDL2 
    OpenGL # GLEW
)

# include(PrintVariables)

# target_link_libraries(vibecheck ${GLUT_LIBRARY}) enable warnings
message(STATUS "compiler id: " ${CXX_COMPILER_ID})
target_compile_options(
  ${PROJECT_NAME}
  PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
    -Wpedantic
    -Wall
    -Wextra>)

# add tests? we can probably steal from the examples? add test subdir
if(ENABLE_TESTING)
  message(STATUS "setup tests")
  enable_testing()
  add_subdirectory(tests)
endif(ENABLE_TESTING)

# ##############################################################################
# INSTALLATION if we are installing
if(CREATE_INSTALL)
  include(GNUInstallDirs)
  set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBIDR}/cmake/guild)

  install(
    TARGETS guild
    EXPORT guild-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/guild
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/guild
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

  # change export name?

  # install header files
  install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR/guild)
  install(FILES ${CMAKE_BINARY_DIR}/include/version.hh
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/guild)

  # Export targets to a script
  install(
    EXPORT guild-targets
    FILE guildTargets.cmake
    DESTINATION ${INSTALL_CONFIGDIR})

  # create a configversion.cmake

  # install not finished!

endif(CREATE_INSTALL)
