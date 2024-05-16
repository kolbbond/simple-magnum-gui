# build imgui with implot
cmake_minimum_required(VERSION 3.10)

#set(CMAKE_C_COMPILER "zig" cc )
#set(CMAKE_CXX_COMPILER "zig" c++)

project(
  guild
  VERSION 0.1
  LANGUAGES C CXX)

message(STATUS "starting cmake generation: " ${PROJECT_NAME})

# switchboard
option(ENABLE_TESTING "build unit/system tests" ON)
option(USE_CMAKEHELPERS "use cmakehelpers for debug" OFF)
option(CREATE_INSTALL "install into general system dir" ON)
option(FLAG_DEBUG "include debug scripts" OFF)

# pick either SDL and OpenGL or Magnum
# we are going to transition to just using Magnum
option(USE_SDL "use SDL for window" OFF)
option(USE_OPENGL "use OpenGL for rendering" OFF)
option(USE_MAGNUM "use magnum and magnum integration libraries" ON)

option(USE_LUA "include lua scripting engine" ON)
option(USE_MATLAB "include matlab scripting engine" OFF)
option(USE_OCTAVE "include octave scripting engine" OFF)
option(USE_PYTHON "include PYTHON scripting engine" OFF)

# if cmake helpers - for debug
if(USE_CMAKEHELPERS)
  list(APPEND CMAKE_MODULE_PATH "$ENV{CMAKEHELPERS_DIR}")
  include(PrintHelpers)
endif(USE_CMAKEHELPERS)

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

# package finding
find_package(Corrade REQUIRED Main)
find_package(Magnum REQUIRED 
    GL
    MeshTools
    Primitives
    Shaders
    Trade
    Sdl2Application
)

find_package(MagnumIntegration REQUIRED ImGui)

# also provide standard SDL OpenGL option?
#find_package(SDL2)
#find_package(OpenGL)

# either include implot or build here
#find_package(ImPlot REQUIRED)
#find_package(MagnumIntegration REQUIRED ImPlot)

# build implot separately?

# set sources for our gui implementation
set(mysources
    src/WindowWrapper.cpp
    src/ImGuiWrapper.cpp
    src/ImPlotWrapper.cpp
    src/GuiMain.cpp
    src/GuiConsole.cpp
    src/GuiBase.cpp
    src/DrawCallback.cpp
    src/RealtimePlot.cpp
    src/log.cpp
    #src/GuiLog.cpp

    # add implot here for now
    implot/implot.cpp
    implot/implot_demo.cpp
    implot/implot_items.cpp
)

# make our library (shared or static?)
add_library(${PROJECT_NAME} SHARED ${mysources})
add_library(guild::guild ALIAS guild)
add_library(guild:: ALIAS guild)

# link them
target_link_libraries(${PROJECT_NAME} PUBLIC
    Corrade::Main
    Magnum::Application
    Magnum::GL
    Magnum::Magnum
    Magnum::MeshTools
    Magnum::Primitives
    Magnum::Shaders
    Magnum::Trade

    MagnumIntegration::ImGui
    #MagnumIntegration::ImPlot
    #ImPlot::ImPlot
)

# Extra packages
# find matlab
if(USE_MATLAB)
    #set(Matlab_ROOT_DIR "$ENV{MATLAB_PATH}")
    PrintVars()
    message(STATUS "find matlab")
    find_package(Matlab REQUIRED)

    if(Matlab_FOUND)
        message(STATUS "found Matlab!")
    endif(Matlab_FOUND)

    target_link_libraries(${PROJECT_NAME} PUBLIC
        Matlab::mex
        Matlab::mx
        Matlab::MatlabEngine
        Matlab::MatlabDataArray
        )

    #set(mysources ${mysources}
        #src/MatlabWrapper.cpp
    #)

endif(USE_MATLAB)

# find lua
if(USE_LUA)
    message(STATUS "find lua")
    find_package(Lua 5.4 REQUIRED)
    if(LUA_FOUND)
        if(NOT TARGET Lua)
            # create a lua library for us
            message(STATUS "Lua is not target")
            add_library(Lua::Lua INTERFACE IMPORTED)
            set_target_properties(Lua::Lua PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${LUA_INCLUDE_DIR}"
                INTERFACE_LINK_LIBRARIES "${LUA_LIBRARIES}")
        endif()

        # link lua to our library
        #target_link_libraries(${PROJECT_NAME} PUBLIC
        #${LUA_LIBRARIES})
        target_link_libraries(${PROJECT_NAME} PUBLIC
            Lua::Lua
            )
    set(mysources ${mysources}
        src/LuaWrapper.cpp
        )
    #target_include_directories(${PROJECT_NAME} PRIVATE ${LUA_INCLUDE_DIR})
    endif()
endif()


# find python
if(USE_PYTHON)

endif()

# add additional sources
target_sources(${PROJECT_NAME} PRIVATE ${mysources})

# at this point we can build everything
# which backend?
# include directories we are making an interface for projects that link to THIS
# library (includes all dependencies in our lib)
target_include_directories(
  ${PROJECT_NAME}
  PUBLIC $<INSTALL_INTERFACE:include>
         $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
         $<BUILD_INTERFACE:${LUA_INCLUDE_DIR}>
 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${LUA_INCLUDE_DIR}
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

# general header
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/guild.hh.in
    ${CMAKE_BINARY_DIR}/include/guild.hh
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
  add_subdirectory(tests)
endif(ENABLE_TESTING)

##################################################
# working executables here?
#add_executable(main tests/test_gui_console.cpp)
#target_link_libraries(main 
#guild)

# RAD directories
# work out of a subdirectory?
add_subdirectory(rad/implot_ex)
add_subdirectory(rad/guibase)

# ##############################################################################
# INSTALLATION if we are installing
if(CREATE_INSTALL)

    # how can we install on windows? ...  @hey
  include(GNUInstallDirs)

  set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/guild)

  install(
    TARGETS guild
    EXPORT guild-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/guild
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/guild
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

  # change export name?
  set_target_properties(guild PROPERTIES EXPORT_NAME guild)

  # install header files
  install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/guild)

  # version header install
  install(FILES ${CMAKE_BINARY_DIR}/include/version.hh
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/guild)

  # general header install
  install(FILES ${CMAKE_BINARY_DIR}/include/guild.hh
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/guild)

    set(_INCLUDE_INSTALL_DIR guild)

  # Export targets to a script
  install(
    EXPORT guild-targets
    FILE guildTargets.cmake
    NAMESPACE guild::
    DESTINATION ${INSTALL_CONFIGDIR}
    )

  # create a configversion.cmake
  include(CMakePackageConfigHelpers)
  write_basic_package_version_file(
      ${CMAKE_CURRENT_BINARY_DIR}/guildConfigVersion.cmake
      VERSION ${PROJECT_VERSION}
      COMPATIBILITY AnyNewerVersion
  )

  configure_package_config_file(
      ${CMAKE_CURRENT_LIST_DIR}/cmake/guildConfig.cmake.in
      ${CMAKE_CURRENT_BINARY_DIR}/guildConfig.cmake
      INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
      PATH_VARS CMAKE_INSTALL_INCLUDEDIR CMAKE_INSTALL_LIBDIR
  )

  # install not finished!
  # install config, configversion and custom find modules
  # (to find with cmake in other projects)
  install (FILES
      ${CMAKE_CURRENT_BINARY_DIR}/guildConfig.cmake
      ${CMAKE_CURRENT_BINARY_DIR}/guildConfigVersion.cmake
      DESTINATION ${INSTALL_CONFIGDIR}
      )

  ################################################## 
  # EXPORT finally
  export(EXPORT guild-targets
      FILE ${CMAKE_CURRENT_BINARY_DIR}/guildTargets.cmake
      )

  # setup install paths
  include(GNUInstallDirs)

  # set rpaths
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
  list(APPEND CMAKE_INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")

  # create debian package?
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
    # include(GenerateDebianPackage)

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
    "* Lua engine                   : ${USE_LUA}\n"
    "* Matlab engine                : ${USE_MATLAB}\n"
    "* Octave engine                : ${USE_OCTAVE}\n"
    "* Python engine                : ${USE_PYTHON}\n"
)
