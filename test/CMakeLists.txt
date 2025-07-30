# testing
message(STATUS "testing")

# add the test files
set(test_list 
    test_magnum_sdl2.cpp
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
  add_executable(${name} ${srcfile})
  target_link_libraries(${name} smg)

  add_test(${name} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${name})
endforeach()

message(STATUS "end of test creation")
