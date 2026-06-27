# testing
message(STATUS "testing")

# Two classes of tests:
#   unit -- headless, no GL context, safe for CI:         ctest -L unit
#   gui  -- construct GuiBase/ScenePanel, need a display: ctest -L gui (opt-in)
# Default CI invocation is `ctest -L unit`; never run unlabeled ctest in CI as
# it would pick up the interactive GUI demos (which return 0 regardless).

option(SMG_BUILD_GUI_TESTS "Build the interactive GUI smoke tests (need a display)" ON)

# icon (Windows only) -- shared by every test target
set(icon_conf "${CMAKE_CURRENT_SOURCE_DIR}/../assets/icon.rc")
set(icon_file "${CMAKE_CURRENT_SOURCE_DIR}/../assets/smg.ico")

function(smg_add_test name srcfile label)
    add_executable(${name} ${srcfile})
    target_link_libraries(${name} smg)

    # let tests locate bundled assets (e.g. cute_goose.jpg) regardless of CWD
    target_compile_definitions(${name} PRIVATE
        SMG_TEST_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

    if(WIN32)
        target_sources(${name} PRIVATE ${icon_conf})
        set_property(TARGET ${name} PROPERTY WIN32_EXECUTABLE_ICON ${icon_file})
    endif()
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES LABELS "${label}")
endfunction()

# --- headless unit tests, Corrade TestSuite harness ------------------------
set(unit_corrade_test_list
    test_drawcallback.cpp
    test_compile_headers.cpp
)
foreach(srcfile ${unit_corrade_test_list})
    string(REPLACE ".cpp" "" name ${srcfile})
    smg_add_test(${name} ${srcfile} "unit")
    target_link_libraries(${name} Corrade::TestSuite)
endforeach()

# --- headless unit tests, lightweight test_util.hh harness -----------------
set(unit_test_list
    test_bounds.cpp
    test_camera.cpp
    test_mesh_bounds.cpp
    test_primitives_axes.cpp
    test_primitives_grid.cpp
    test_sprite_types.cpp
    test_sprite_anim.cpp
    test_iso_grid.cpp
    test_camera_pick.cpp
)
foreach(srcfile ${unit_test_list})
    string(REPLACE ".cpp" "" name ${srcfile})
    smg_add_test(${name} ${srcfile} "unit")
endforeach()

# --- interactive GUI smoke tests (opt-in, need a display) ------------------
if(SMG_BUILD_GUI_TESTS)
    set(gui_test_list
        test_magnum_sdl2.cpp
        test_magnum_application_main.cpp
        test_magnum_imgui.cpp
        test_magnum_implot.cpp
        test_maptotexture.cpp
        test_smg_guibase.cpp
        test_smg_guibase_looped.cpp
        test_plottris.cpp
        test_plotquads.cpp
        test_smg_libraries.cpp
        test_scene_panel.cpp
        test_scene_bloom.cpp
        test_file_dialog.cpp
        test_sprite_sheet.cpp
        test_sprite_renderer.cpp
    )
    foreach(srcfile ${gui_test_list})
        string(REPLACE ".cpp" "" name ${srcfile})
        smg_add_test(${name} ${srcfile} "gui")
    endforeach()

    # the sprite renderer is an internal smg component (header under src/)
    target_include_directories(test_sprite_renderer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
endif()

message(STATUS "end of test creation")
