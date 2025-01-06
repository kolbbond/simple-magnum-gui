# Encapsulated build of gui parts
# Simple-magnum-gui (SMG)

A simple gui to generate a window and allow plotting.
Great for quick debugging and drawing of data. 
Magnum gives easy opengl support.

### Dependencies

This utilizes Dear ImGui, Magnum, ImPlot.
* First build Magnum
    * [Corrade](github.com/mosra/corrade)
    * [Magnum](github.com/mosra/magnum)
        * make sure to build with SDL2

* We forked magnum-integration and added ImGui
    *    [magnum-integration](https://github.com/kolbbond/magnum-integration)
        * clone the ImGui repo within the src/MagnumExternal directory
        (see the clone_repos.sh script)

 ## utilizing the library
  See the examples
 * Start the gui window with GuiBase
 * create a DrawCallback
    * set your static callback function with prototype 
        ` int callback_function(void* data); `
    * set any events (mouse move, mouse scroll, key press) with the relevant prototypes
    * set your data
* add the callback to your guibase object
* run your game loop and call the gui function `mainLoopIteration`






