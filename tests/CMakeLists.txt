# testing

# add the test files
set(test_list 
    test_sdl2_opengl3.cpp
    test_implot.cpp
)

# walk over source files
foreach(srcfile ${test_list})
  string(REPLACE ".cpp" "" name ${srcfile})
  add_executable(${name} ${srcfile})
  #target_link_libraries(${name} guild ${SDL_LIB})
  target_link_libraries(${name} guild)

  add_test(${name} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${name})
endforeach()
