set(imgui_commit "42015f71940e3547422ca7870e2d2d44ad499fae")

# if no imgui 
if(NOT EXISTS "${imgui_path}/.git")
  message(STATUS "Cloning imgui...")

  # checkout imgui
  execute_process(COMMAND
      #git clone --branch v1.91.9 --depth 1 https://github.com/ocornut/imgui.git ${imgui_path}
    git clone --branch docking --depth 1 https://github.com/ocornut/imgui.git ${imgui_path}
    RESULT_VARIABLE res
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
  )
  if(NOT res EQUAL 0)
    message(FATAL_ERROR "Git clone failed: ${err}")
  endif()

    # checkout specific commit
    execute_process(
      COMMAND git fetch origin ${imgui_commit}
      WORKING_DIRECTORY ${imgui_path}
    )

    execute_process(
      COMMAND git checkout ${imgui_commit}
      WORKING_DIRECTORY ${imgui_path}
    )

else()
  message(STATUS "imgui already exists, skipping clone")
endif()

