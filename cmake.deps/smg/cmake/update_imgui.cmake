if(NOT EXISTS "${imgui_path}/.git")
  message(STATUS "Cloning imgui...")
  execute_process(COMMAND
    git clone --branch v1.91.9 --depth 1 https://github.com/ocornut/imgui.git ${imgui_path}
    RESULT_VARIABLE res
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
  )
  if(NOT res EQUAL 0)
    message(FATAL_ERROR "Git clone failed: ${err}")
  endif()
else()
  message(STATUS "imgui already exists, skipping clone")
endif()
