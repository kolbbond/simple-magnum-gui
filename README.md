# Simple-Magnum-Gui (SMG)

A simple gui to generate a window and allow plotting.
Great for quick debugging and drawing of data. 

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

These are encapsulated into the dependencies directory.  
The cmake commands are implemented in shell script wrappers in the
`scripts` directory. 
FYI both of the above scripts install to `~/.local`. 

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






