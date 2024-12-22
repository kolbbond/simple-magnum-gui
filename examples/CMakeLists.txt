project(examples)

add_executable(implot_ex implot_ex.cpp)

target_link_libraries(implot_ex
    smg::smg
    )

