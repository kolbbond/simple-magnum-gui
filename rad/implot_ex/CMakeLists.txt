project(implot_ex)

add_executable(${PROJECT_NAME} implot_ex.cpp)

target_link_libraries(${PROJECT_NAME}
    guild::guild
    )

