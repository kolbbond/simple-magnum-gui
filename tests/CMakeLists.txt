# testing

message(STATUS "testing")

# add the test files
set(test_list 
    test_magnum_sdl2.cpp
    test_magnum_application_main.cpp
    test_magnum_imgui.cpp
    #test_magnum_implot.cpp
    test_guibase.cpp
    test_loadimage.cpp
    test_plottris.cpp
    test_maptotexture.cpp
)

# package specific
if(USE_LUA)

        set(test_list ${test_list} 
            test_lua_api.cpp
            )

endif(USE_LUA)

if(USE_MATLAB)
        set(test_list ${test_list} 
            test_matlab_api.cpp
            )
endif(USE_MATLAB)

# walk over source files
foreach(srcfile ${test_list})
  string(REPLACE ".cpp" "" name ${srcfile})
  add_executable(${name} ${srcfile})
  target_link_libraries(${name} guild)

  add_test(${name} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${name})
endforeach()

message(STATUS "end of test creation")
