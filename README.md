# Encapsulated build of gui parts
# guild 

gui linker -> guild

This turned into a straight up GUI library
The dependent libraries that this is built on are included as sub-dirs

1. Optional scripting languages
    * Lua
    * Matlab
    * (Python?)

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

* Clone implot within this directory (we might change to include in magnum-integration)
    *     [implot](github.com/epezent/implot)

 ## This is getting complicated!
 then build with the usual cmake process.

 ## utilizing the library
  see the tests but general idea is
 * Start the gui window with GuiBase
 * create a DrawCallback
    * set your static callback function with prototype 
        ` int callback_function(void* data); `
    * set any events (mouse move, mouse scroll, key press) with the relevant prototypes
    * set your data
* add the callback to your guibase object
* run your game loop and call the gui function `mainLoopIteration`






