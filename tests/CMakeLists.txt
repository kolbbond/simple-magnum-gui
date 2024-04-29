# testing

# add the test files
set(test_list 
    test_sdl2_opengl3.cpp
    test_implot.cpp
    test_imgui_demos.cpp
    test_imgui_wrapper.cpp
    test_sdl_window.cpp
    test_window_ptr.cpp
    test_window_wrapper.cpp
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
  #target_link_libraries(${name} guild ${SDL_LIB})
  target_link_libraries(${name} guild)

  add_test(${name} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${name})
endforeach()
