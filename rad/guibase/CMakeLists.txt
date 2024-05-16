project(guibase)

add_executable(${PROJECT_NAME} guibase.cpp)
add_executable(draw_triangle draw_triangle.cpp)
add_executable(draw_cube draw_cube.cpp)

target_link_libraries(guibase guild::guild   )
target_link_libraries(draw_triangle  guild::guild   )
target_link_libraries(draw_cube  guild::guild   )

