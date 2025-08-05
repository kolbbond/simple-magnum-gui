project(examples)

# testing
message(STATUS "compile examples")

# add the test files
set(example_list 
    implot_ex.cpp
    draw_cube.cpp
    draw_triangle.cpp
    guibase.cpp
)

# walk over source files
foreach(srcfile ${example_list})
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

endforeach()

message(STATUS "end of example creation")

