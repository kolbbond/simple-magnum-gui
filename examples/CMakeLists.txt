project(examples)

add_executable(implot_ex implot_ex.cpp)
target_link_libraries(implot_ex
    smg::smg
    )

add_executable(draw_cube draw_cube.cpp)
target_link_libraries(draw_cube
    smg::smg
    )

