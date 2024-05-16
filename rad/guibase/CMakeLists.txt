project(guibase)

add_executable(${PROJECT_NAME} guibase.cpp)

target_link_libraries(${PROJECT_NAME}
    guild::guild
    )

