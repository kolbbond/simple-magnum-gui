smg examples
============

Example programs for simple-magnum-gui (smg): ImGui on Magnum/OpenGL.
Everything they need is in bin/, so the folder can be copied or unzipped anywhere.

  bin\guibase.exe        bare GuiBase window with the ImGui demo
  bin\draw_triangle.exe  a Magnum mesh drawn from a draw callback
  bin\draw_cube.exe      3D cube with a camera
  bin\implot_ex.exe      ImPlot / ImPlot3D plotting
  bin\scene_panel.exe    ScenePanel 3D viewport (orbit camera, bloom)
  bin\sprite_panel.exe   iso sprites in a ScenePanel
  bin\file_dialog.exe    the smg file dialog
  bin\sysinfo.exe        system monitor: CPU per core, threads, memory, processes, disks, network, GPU

Requires an OpenGL 3.3 capable GPU driver.
Every example accepts --smg-frames N to exit after N frames.
