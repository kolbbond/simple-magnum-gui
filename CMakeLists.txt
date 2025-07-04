# build imgui with magnum
cmake_minimum_required(VERSION 3.11)

project(
  simple-magnum-gui 
  VERSION 0.1
  LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message(STATUS "starting cmake generation: " ${PROJECT_NAME})

# switchboard
option(BUILD_DEPENDENCIES "build dependencies and install" OFF)
option(ENABLE_ARMADILLO "build with armadillo" ON)
option(ENABLE_TESTING "build unit/system tests" ON)
option(ENABLE_EXAMPLES "build examples" ON)
option(ENABLE_IMPLOT3D "build IMPLOT3D" ON)
option(CREATE_INSTALL "install into general system dir" ON)
option(FLAG_DEBUG "include debug scripts" OFF)

# we are going to transition to just using Magnum
# right now just magnum works
option(USE_MAGNUM "use magnum and magnum integration libraries" ON)
option(USE_X "use X for window" OFF)
option(USE_SDL "use SDL for window" OFF)
option(USE_OPENGL "use OpenGL for rendering" OFF)

# set export commands
#set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/.local/")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" 
    ${CMAKE_MODULE_PATH})

# system detection
if(WIN32)
    message(STATUS "this is a Windows machine")
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

# output paths
if(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY AND NOT DEFINED CMAKE_LIBRARY_OUTPUT_DIRECTORY AND NOT DEFINED CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/bin CACHE PATH "" FORCE)
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/lib CACHE PATH "" FORCE)
	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/lib CACHE PATH "" FORCE)
endif()

message(STATUS "building in ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

# add dependencies
if(BUILD_DEPENDENCIES)
    add_subdirectory(dependencies)
endif()

# package finding
find_package(Corrade REQUIRED Main Utility)
find_package(Magnum REQUIRED 
    GL
    MeshTools
    Primitives
    Shaders
    Trade
    Sdl2Application
    MagnumFont
    Text
)

find_package(MagnumIntegration REQUIRED ImGui)

if(ENABLE_ARMADILLO)
# find armadillo library
    find_package(Armadillo 14.0.0 REQUIRED)
    if(NOT TARGET Arma::Armadillo)
        # create target
        add_library(Arma::Armadillo INTERFACE IMPORTED)
        set_target_properties(Arma::Armadillo PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${ARMADILLO_INCLUDE_DIRS}"
            INTERFACE_LINK_LIBRARIES "${ARMADILLO_LIBRARY}")
    endif()

endif()

# either include implot or build here
#find_package(ImPlot REQUIRED)

# set sources for our gui implementation
set(mysources
    src/GuiBase.cpp
    src/DrawCallback.cpp
    src/log.cpp
    src/Statics.cpp

    # if we have armadillo
    src/Plotter.cpp

    # add implot here for now
    implot/implot.cpp
    implot/implot_demo.cpp
    implot/implot_items.cpp
)

# for implot3d
if(ENABLE_IMPLOT3D)
    list(APPEND mysources
        implot3d/implot3d.cpp
        implot3d/implot3d_items.cpp
        implot3d/implot3d_demo.cpp
        implot3d/implot3d_meshes.cpp
    )
endif()

corrade_add_resource(smg_RESOURCES "assets/resources.conf")

# make our library (shared or static?)
add_library(smg SHARED ${mysources} ${smg_RESOURCES})
add_library(smg::smg ALIAS smg)
add_library(smg:: ALIAS smg)

# link them
target_link_libraries(smg PUBLIC
    Corrade::Main
    Magnum::Application
    Magnum::GL
    Corrade::Utility
    Magnum::Magnum
    Magnum::MeshTools
    Magnum::Primitives
    Magnum::Shaders
    Magnum::Trade
    Magnum::Text

    MagnumIntegration::ImGui
    #ImPlot::ImPlot
)

if(ENABLE_ARMADILLO)
    target_link_libraries(smg PUBLIC
        Arma::Armadillo)
endif()

# add additional sources
target_sources(smg PRIVATE ${mysources})

# at this point we can build everything
# include directories we are making an interface for projects that link to THIS
# library (includes all dependencies in our lib)
target_include_directories(
  smg
  PUBLIC $<INSTALL_INTERFACE:include>
         $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/implot>
         $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/ImGuiFileDialog>
         $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(ENABLE_IMPLOT3D)
target_include_directories(
    smg PUBLIC
     $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/implot3d>
 )
 endif()

message(STATUS "compiler id: " ${CXX_COMPILER_ID})
target_compile_options(
  smg
  PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
    -Wpedantic
    -Wall
    -Wextra>)

# general header
configure_file(
    ${PROJECT_SOURCE_DIR}/cmake/smg.hh.in
    ${CMAKE_BINARY_DIR}/include/smg.hh
    )

# version header
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/version.hh.in
    ${CMAKE_BINARY_DIR}/include/version.hh
    )

# add tests? we can probably steal from the examples? add test subdir
if(ENABLE_TESTING)
  message(STATUS "setup tests")
  enable_testing()
  add_subdirectory(test)
endif(ENABLE_TESTING)

if(ENABLE_EXAMPLES)
    message(STATUS "build examples")
    add_subdirectory(examples)
endif()

# ##############################################################################
# INSTALLATION if we are installing
if(CREATE_INSTALL)

    # how can we install on windows? ...  @hey
  include(GNUInstallDirs)

  set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/smg)

  install(
    TARGETS smg
    EXPORT smg-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/smg
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/smg
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

  # change export name?
  set_target_properties(smg PROPERTIES EXPORT_NAME smg)

  # install header files
  install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/smg)

  # version header install
  install(FILES ${CMAKE_BINARY_DIR}/include/version.hh
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/smg)

  # general header install
  install(FILES ${CMAKE_BINARY_DIR}/include/smg.hh
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/smg)

  # add implot headers
  install(FILES 
      ${CMAKE_SOURCE_DIR}/implot/implot.h
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/smg)

      # add implot3d headers
    if(ENABLE_IMPLOT3D)
      install(FILES 
          ${CMAKE_SOURCE_DIR}/implot3d/implot3d.h
              DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/smg)
      endif()

    set(_INCLUDE_INSTALL_DIR smg)

  # Export targets to a script
  install(
    EXPORT smg-targets
    FILE smgTargets.cmake
    NAMESPACE smg::
    DESTINATION ${INSTALL_CONFIGDIR}
    )

  # create a configversion.cmake
  include(CMakePackageConfigHelpers)
  write_basic_package_version_file(
      ${CMAKE_CURRENT_BINARY_DIR}/smgConfigVersion.cmake
      VERSION smg
      COMPATIBILITY AnyNewerVersion
  )

  configure_package_config_file(
      ${CMAKE_CURRENT_LIST_DIR}/cmake/smgConfig.cmake.in
      ${CMAKE_CURRENT_BINARY_DIR}/smgConfig.cmake
      INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
      PATH_VARS CMAKE_INSTALL_INCLUDEDIR CMAKE_INSTALL_LIBDIR
  )

  # install not finished!
  # install config, configversion and custom find modules
  # (to find with cmake in other projects)
  install (FILES
      ${CMAKE_CURRENT_BINARY_DIR}/smgConfig.cmake
      ${CMAKE_CURRENT_BINARY_DIR}/smgConfigVersion.cmake
      # install dependency modules too?
      ${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindImGui.cmake
      DESTINATION ${INSTALL_CONFIGDIR}
      )

  ################################################## 
  # EXPORT finally
  export(EXPORT smg-targets
      FILE ${CMAKE_CURRENT_BINARY_DIR}/smgTargets.cmake
      )

  # setup install paths
  include(GNUInstallDirs)

  # set rpaths
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
  list(APPEND CMAKE_INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")

  # create debian package?
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

endif(CREATE_INSTALL)


# cmake summary
message("\n"
    "Installation Summary\n"
    "--------------------\n"
    "* Install Directory            : ${CMAKE_INSTALL_PREFIX}\n"
    "* Build Type                   : ${CMAKE_BUILD_TYPE}\n"
    "* Build static libs            : ${BUILD_STATIC_LIBS}\n"
    "* Build tests                  : ${ENABLE_TESTING}\n"
    "* Magnum                       : ${USE_MAGNUM}\n"
)
