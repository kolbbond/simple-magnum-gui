
#.rst:
if(CORRADE_TARGET_APPLE)
    find_library(_IMPLOT_ApplicationServices_LIBRARY ApplicationServices)
    mark_as_advanced(_IMPLOT_ApplicationServices_LIBRARY)
    set(_IMPLOT_EXTRA_LIBRARIES ${_IMPLOT_ApplicationServices_LIBRARY})

# Since 1.82, ImPlot on MinGW needs the imm32 library. For MSVC the library
# seems to be linked implicitly so this is not needed.
elseif(CORRADE_TARGET_WINDOWS AND CORRADE_TARGET_MINGW)
    set(_IMPLOT_EXTRA_LIBRARIES imm32)
endif()

# Vcpkg distributes implot as a library with a config file, so try that first --
# but only if IMPLOT_DIR wasn't explicitly passed, in which case we'll look
# there instead
if(NOT IMPLOT_DIR AND NOT TARGET implot::implot)
    find_package(implot CONFIG QUIET)
endif()
if(NOT IMPLOT_DIR AND TARGET implot::implot)
    if(NOT TARGET ImPlot::ImPlot)
        add_library(ImPlot::ImPlot INTERFACE IMPORTED)
        set_property(TARGET ImPlot::ImPlot APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES implot::implot ${_IMPLOT_EXTRA_LIBRARIES})

        # Retrieve include directory for FindPackageHandleStandardArgs later
        get_target_property(ImPlot_INCLUDE_DIR implot::implot
            INTERFACE_INCLUDE_DIRECTORIES)

        add_library(ImPlot::Sources INTERFACE IMPORTED)
        set_property(TARGET ImPlot::Sources APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES ImPlot::ImPlot)
    endif()

# Otherwise find the source files and compile them as part of the library they
# get linked to
else()
    # Disable the find root path here, it overrides the
    # CMAKE_FIND_ROOT_PATH_MODE_INCLUDE setting potentially set in
    # toolchains.
    find_path(ImPlot_INCLUDE_DIR NAMES implot.h
        HINTS ${IMPLOT_DIR}
        PATH_SUFFIXES MagnumExternal/ImPlot
        NO_CMAKE_FIND_ROOT_PATH)
    mark_as_advanced(ImPlot_INCLUDE_DIR)

    if(NOT TARGET ImPlot::ImPlot)
        add_library(ImPlot::ImPlot INTERFACE IMPORTED)
        set_property(TARGET ImPlot::ImPlot APPEND PROPERTY
            INTERFACE_INCLUDE_DIRECTORIES ${ImPlot_INCLUDE_DIR})
        if(_IMPLOT_EXTRA_LIBRARIES)
            set_property(TARGET ImPlot::ImPlot APPEND PROPERTY
                INTERFACE_LINK_LIBRARIES ${_IMPLOT_EXTRA_LIBRARIES})
        endif()

        # Handle export and import of implot symbols via IMPLOT_API definition
        # in visibility.h of Magnum ImPlotIntegration.
        set_property(TARGET ImPlot::ImPlot APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS
            "IMPLOT_USER_CONFIG=\"Magnum/ImPlotIntegration/visibility.h\"")
    endif()
endif()

macro(_implot_setup_source_file source_var)
    # Handle export and import of implot symbols via IMPLOT_API
    # definition in visibility.h of Magnum ImPlotIntegration.
    set_property(SOURCE ${${source_var}} APPEND PROPERTY COMPILE_DEFINITIONS
        "IMPLOT_USER_CONFIG=\"Magnum/ImPlotIntegration/visibility.h\"")

    # Hide warnings from implot source files

    # GCC- and Clang-specific flags
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR (CMAKE_CXX_COMPILER_ID MATCHES "(Apple)?Clang"
        AND NOT CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC") OR CORRADE_TARGET_EMSCRIPTEN)
        set_property(SOURCE ${${source_var}} APPEND_STRING PROPERTY COMPILE_FLAGS
            " -Wno-old-style-cast")
    endif()

    # GCC-specific flags
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set_property(SOURCE ${${source_var}} APPEND_STRING PROPERTY COMPILE_FLAGS
            " -Wno-double-promotion -Wno-zero-as-null-pointer-constant")
    endif()

    mark_as_advanced(${source_var})
endmacro()

# Find components
foreach(_component IN LISTS ImPlot_FIND_COMPONENTS)
    if(_component STREQUAL "Sources")
        if(NOT TARGET ImPlot::Sources)
            set(ImPlot_Sources_FOUND TRUE)
            set(ImPlot_SOURCES )


            foreach(_file implot implot_items implot_demo)
                # Disable the find root path here, it overrides the
                # CMAKE_FIND_ROOT_PATH_MODE_INCLUDE setting potentially set in
                # toolchains.
                find_file(ImPlot_${_file}_SOURCE NAMES ${_file}.cpp
                    HINTS ${IMPLOT_DIR} NO_CMAKE_FIND_ROOT_PATH)

                if(NOT ImPlot_${_file}_SOURCE)
                    set(ImPlot_Sources_FOUND FALSE)
                    break()
                endif()

                list(APPEND ImPlot_SOURCES ${ImPlot_${_file}_SOURCE})
                _implot_setup_source_file(ImPlot_${_file}_SOURCE)
            endforeach()

            message("make ImPlot")

            add_library(ImPlot::Sources INTERFACE IMPORTED)
            set_property(TARGET ImPlot::Sources APPEND PROPERTY
                INTERFACE_SOURCES "${ImPlot_SOURCES}")
            set_property(TARGET ImPlot::Sources APPEND PROPERTY
                INTERFACE_LINK_LIBRARIES ImPlot::ImPlot)

            find_package(ImGui REQUIRED)

            target_link_libraries(ImPlot::Sources
                INTERFACE
                    ImGui::ImGui
                    )
        else()
            set(ImPlot_Sources_FOUND TRUE)
        endif()
    endif()
endforeach()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ImPlot
    REQUIRED_VARS ImPlot_INCLUDE_DIR HANDLE_COMPONENTS)
