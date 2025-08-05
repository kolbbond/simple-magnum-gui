# testing
message(STATUS "testing")

# add the test files
set(test_list 
    #    test_magnum_sdl2.cpp
    test_magnum_application_main.cpp
    test_magnum_imgui.cpp
    test_smg_guibase.cpp
    test_smg_guibase_looped.cpp
    test_plottris.cpp
    test_plotquads.cpp
    #test_magnum_implot.cpp
    test_implot.cpp
    test_mesh_destruction.cpp
    #test_loadimage.cpp
    #test_maptotexture.cpp
)

# walk over source files
foreach(srcfile ${test_list})
  string(REPLACE ".cpp" "" name ${srcfile})

# create the executable
    add_executable(${name} ${srcfile})
    target_link_libraries(${name} smg)

  # add icon
    set(icon_conf "${CMAKE_CURRENT_SOURCE_DIR}/../assets/icon.rc")
    set(icon_file "${CMAKE_CURRENT_SOURCE_DIR}/../assets/smg.ico")

    if(WIN32)
        #  set_property(TARGET ${name} PROPERTY WIN32_EXECUTABLE TRUE)
        target_sources(${name} PRIVATE ${icon_conf})
        set_property(TARGET ${name} PROPERTY WIN32_EXECUTABLE_ICON ${icon_file})
    endif()


    add_test(${name} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${name})
endforeach()

message(STATUS "end of test creation")
