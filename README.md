# Simple-Magnum-Gui (SMG)

A simple gui to generate a window and allow plotting.
Great for quick debugging and drawing of data. 

![smg example](assets/smg_example.png)

Magnum gives easy opengl support.

Make sure to clone the submodules too.

`git submodule init`
`git submodule update`

### Dependencies

This utilizes Dear ImGui, Magnum, ImPlot.  
[Corrade](github.com/mosra/corrade)  
[Magnum](github.com/mosra/magnum)  
[magnum-integration](https://github.com/mosra/magnum-integration)  
[imgui](https://github.com/ocornut/imgui)  * cloned into src/MagnumExternal/ImGui directory
add implot and implot3

### Linux dependencies
Build with the `make deps`
and `make`
make sure to add a LD_LIBRARY_PATH
`export LD_LIBRARY_PATH=/home/you/programs/simple-magnum-gui/.deps/usr/lib:${LD_LIBRARY_PATH}`
to find the libraries

#### Windows dependencies
Use vcpkg (classic mode) for SDL2/Freetype/libjpeg-turbo:

```
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
$env:VCPKG_ROOT = "$pwd"
[Environment]::SetEnvironmentVariable("VCPKG_ROOT", $env:VCPKG_ROOT, "User")
& "$env:VCPKG_ROOT\vcpkg.exe" install sdl2 freetype libjpeg-turbo
```

Open an x64 MSVC environment (Developer PowerShell or DevShell) and ensure
`ninja` is on PATH, then build:

```
cmake --preset windows-vcpkg
cmake --build build --config Debug
```

If you run from the build tree, set plugin and SDL2 paths:

```
$env:MAGNUM_PLUGINS_DEBUG_DIR="$pwd\build\bin\magnum-d"
$env:PATH="$env:VCPKG_ROOT\installed\x64-windows\debug\bin;$env:PATH"
```

## legacy dependencies
These are Linux-focused scripts in `scripts` and install to `~/.local`.

 ## utilizing the library
Gui uses a naive callback system to enable different systems.
See the examples
 * Start the gui window with GuiBase
 * create a DrawCallback
    * set your static callback function with prototype 
        ` int callback_function(void* data); `
    * set any events (mouse move, mouse scroll, key press) with the relevant prototypes
    * set your data
* add the callback to your guibase object
* run your game loop and call the gui function `mainLoopIteration`





