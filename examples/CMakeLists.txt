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
    endif()

endforeach()

message(STATUS "end of example creation")

