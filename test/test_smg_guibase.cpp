// implot testing
#include "GuiBase.hh"
#include "DrawCallback.hh"
#include "gui_test_util.hh"
#include "imgui.h"

#include <cstring>
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

int callback_fun(const data_ex& /*data*/) {
    printf("debug callback\n");

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

int main(int argc, char** argv) {

    // make our application class
    printf("make guibase application\n");
    GuiBase gui({ argc, argv });
    smgtest::frame_limit(gui);

    // example data
    data_ex mydata = data_ex();
    mydata.x = 5;
    mydata.y = 6;
    mydata.name = "heymom";

    gui.add_callback([&mydata]() { return callback_fun(mydata); });

    // -g keeps the window open until closed; otherwise run a single frame
    const bool keep_open = argc == 2 && strcmp(argv[1], "-g") == 0;
    bool done = false;
    while(!done) {
        printf("loop iteration\n");
        done = !gui.mainLoopIteration() || !keep_open;
    }

    // exit
    gui.exit();
}
