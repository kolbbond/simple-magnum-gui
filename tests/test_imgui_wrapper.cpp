// window wrapper test

//#include "SDL.h"
#include "ImGuiWrapper.hh"
#include "WindowWrapper.hh"

int main(){

    printf("window wrapper test\n");

    // create a shared pointer
    ShWindowWrapperPr window = WindowWrapper::create();

    printf("window ptr init\n");
    window->init();

    printf("imgui ptr init\n");
    ShImGuiWrapperPr imgui = ImGuiWrapper::create();
    imgui->init(window);

    printf("imgui ptr close\n");
    imgui->close();

    printf("window ptr close\n");
    window->close();

    //SDL_Quit();

}
