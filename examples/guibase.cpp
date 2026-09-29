// implot testing
#include "GuiBase.hh"
#include "DrawCallback.hh"
#include "imgui.h"

#include <iostream>

using namespace Magnum;

// example data to pass into callback
class data_ex {
public:
    int x;
    int y;

    std::string name = "example";
};

using namespace smg;

int callback_fun(const data_ex& mydata) {
    (void)mydata;

    ImGui::Begin("hey mom");
    ImGui::Text("Hello, world!");
    if(ImGui::Button("Test Window")) {}
    if(ImGui::Button("Another Window")) {}
    ImGui::Text(
        "Application average %.3f ms/frame (%.1f FPS)", 1000.0 / Double(ImGui::GetIO().Framerate), Double(ImGui::GetIO().Framerate));
    ImGui::End();

    // 0 means success
    return 0;
}

// Custom application class that sets up callback in constructor
class GuiBaseExample: public GuiBase {
public:
    explicit GuiBaseExample(const Arguments& arguments) : GuiBase(arguments) {
        // example data
        _mydata.x = 5;
        _mydata.y = 6;
        _mydata.name = "heymom";

        add_callback([this]() { return callback_fun(_mydata); });
    }

private:
    data_ex _mydata;
};

// Use MAGNUM_APPLICATION_MAIN for proper cross-platform main loop
// This handles Emscripten's event loop correctly
MAGNUM_APPLICATION_MAIN(GuiBaseExample)
