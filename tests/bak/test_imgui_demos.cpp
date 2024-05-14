// window wrapper test

//#include "SDL.h"
//#include "ImGuiWrapper.hh"
//#include "WindowWrapper.hh"
#include "GuiMain.hh"

int main(){

    printf("gui main test\n");

    // create a shared pointer
    ShWindowWrapperPr window = WindowWrapper::create();
    window->init();

    ShImGuiWrapperPr imgui = ImGuiWrapper::create();
    imgui->init(window);

    ShImPlotWrapperPr implot = ImPlotWrapper::create();
    implot->init();

    ShGuiMainPr gui = GuiMain::create();
    //gui->init();

    gui->set_window(window);
    gui->set_imgui(imgui);
    gui->set_implot(implot);

    // game loop here

    gui->handle_events();
    gui->new_frame();
    gui->demo_window();

    printf("close EVERYTHING!\n");
    imgui->close();
    window->close();
    implot->close();
    //gui->close();

    //SDL_Quit();

}
