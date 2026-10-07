project(examples)

# testing
message(STATUS "compile examples")

# add the test files
if(EMSCRIPTEN)
    # Only guibase uses MAGNUM_APPLICATION_MAIN, others use mainLoopIteration()
    set(example_list
        guibase.cpp
    )
else()
    set(example_list
        implot_ex.cpp
        draw_cube.cpp
        draw_triangle.cpp
        guibase.cpp
        scene_panel.cpp
        file_dialog.cpp
        sprite_panel.cpp
        sysinfo.cpp
        overlay.cpp
        visualizer.cpp
        files.cpp
    )
endif()

# WASM-specific linker flags
if(EMSCRIPTEN)
    set(WASM_LINK_FLAGS
        "-s USE_SDL=2"
        "-s USE_WEBGL2=1"
        "-s ALLOW_MEMORY_GROWTH=1"
        "-s FULL_ES3=1"
        "-s MIN_WEBGL_VERSION=2"
        "-s MAX_WEBGL_VERSION=2"
        "--bind"
    )
    string(REPLACE ";" " " WASM_LINK_FLAGS_STR "${WASM_LINK_FLAGS}")
endif()

# walk over source files
foreach(srcfile ${example_list})
  string(REPLACE ".cpp" "" name ${srcfile})

# create the executable
    add_executable(${name} ${srcfile})
    target_link_libraries(${name} smg)

    if(EMSCRIPTEN)
        # WASM output settings
        set_target_properties(${name} PROPERTIES
            SUFFIX ".html"
            LINK_FLAGS "${WASM_LINK_FLAGS_STR}"
        )
    else()
        # Desktop: add icon
        set(icon_conf "${CMAKE_CURRENT_SOURCE_DIR}/../assets/icon.rc")
        set(icon_file "${CMAKE_CURRENT_SOURCE_DIR}/../assets/smg.ico")

        if(WIN32)
            target_sources(${name} PRIVATE ${icon_conf})
            set_property(TARGET ${name} PROPERTY WIN32_EXECUTABLE_ICON ${icon_file})
        endif()

        # end-user examples package (cmake/PackageExamples.cmake)
        install(TARGETS ${name} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT examples)
        set_property(GLOBAL APPEND PROPERTY SMG_EXAMPLE_TARGETS ${name})
    endif()

endforeach()

message(STATUS "end of example creation")

# sysinfo reads per-core load, clocks, network and adapters through these
if(TARGET sysinfo AND WIN32)
    target_link_libraries(sysinfo iphlpapi powrprof dxgi)
endif()

# the overlay is a desktop widget: no console window behind it
if(TARGET overlay AND MSVC)
    target_link_options(overlay PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
endif()

# every example renders a few frames off-screen and saves a screenshot: a smoke test that needs
# no visible window, and the PNGs (build/examples/shots) show what each one drew
if(ENABLE_TESTING AND NOT EMSCRIPTEN)
    get_property(_examples GLOBAL PROPERTY SMG_EXAMPLE_TARGETS)
    foreach(_e ${_examples})
        set(_args --smg-hidden --smg-frames 30 --smg-screenshot ${CMAKE_CURRENT_BINARY_DIR}/shots/${_e}.png)
        if(_e STREQUAL "visualizer")
            list(APPEND _args --tone) # CI machines have no audio device
        endif()
        add_test(NAME example_${_e} COMMAND ${_e} ${_args})
        set_tests_properties(example_${_e} PROPERTIES LABELS "example" TIMEOUT 60)
        if(WIN32)
            # Windows has no rpath: put the dependency DLLs on PATH
            set(_env "")
            foreach(_prefix ${CMAKE_PREFIX_PATH})
                file(TO_CMAKE_PATH "${_prefix}" _prefix)
                list(APPEND _env "PATH=path_list_prepend:${_prefix}/bin")
            endforeach()
            if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
                list(APPEND _env "PATH=path_list_prepend:${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin")
            endif()
            set_tests_properties(example_${_e} PROPERTIES ENVIRONMENT_MODIFICATION "${_env}")
        endif()
    endforeach()
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/shots)
endif()
