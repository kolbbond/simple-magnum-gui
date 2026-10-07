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
  bin\overlay.exe        transparent always-on-top HUD; drag to move, Ctrl+Alt+O toggles click-through
  bin\visualizer.exe     audio spectrum + waveform from the default input; --wav file.wav, --tone
  bin\files.exe          fast file browser: filter, recursive search, text/image preview
  bin\csvplot.exe        plot CSV columns: drop files on the window, cursor readout, follow live files
  bin\diskusage.exe      disk usage: size-sorted tree, treemap, biggest file types

Requires an OpenGL 3.3 capable GPU driver.
Every example accepts:
  --smg-frames N           exit after N frames
  --smg-screenshot F.png   save the last frame (default after 60 frames) and exit
  --smg-record DIR         save every frame as DIR/frame_00000.png
  --smg-hidden             render off-screen, no window
