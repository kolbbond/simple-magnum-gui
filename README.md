# Encapsulated build of gui parts
# guild 
* the name is cute gui (ld) linker?
1. (i cant spell)

This turned into a straight up GUI library
The dependent libraries that this is built on are included as sub-dirs
so you can clone/fork those repos yourself
1. Now we have scripting languages
    * Lua
    * Matlab
    * (Python?)

My attempt at a build system for a combined gui library. 
These are amazing gui libraries to utilize but 
* These guys did not supply a build system <br />
 so here it is.
 1. imgui: awesome lightweight immediate mode gui, for the window 
 2. implot: to aid in plotting, for science mostly
 3. magnum: graphics for 3D
 4. magnum-integration: magnum with imgui
 5 ...?

 The dependencies, git clone into the sub directories
 1. imgui: 'git@github.com:ocornut/imgui.git'
 1. implot: 'git@github.com:epezent/implot.git'
 1. corrade: `git@github.com:mosra/corrade.git`
 1. magnum: 'git@github.com:mosra/magnum.git'
 1. magnum-integration: 'git@github.com:mosra/magnum-integration.git'

 ## This is getting complicated!

 * install corrade
 * install magnum
    * make sure to build with SDL2
 * clone magnum-integration
    * set cmake to build with imgui
    * cd into src/MagnumExternal
    * git clone imgui here, and rename > ImGui
        * optional: git clone implot and `cp implot/implot* Imgui/. `
    *   * if implot also add > implot.h and implot_external.h to cmakelists `
    * go to modules/FindImGui.cmake and add the  
    > implot implot_items implot_demo 
    to the foreach at line 156..?
    * this should be enough to install

* make guild?




