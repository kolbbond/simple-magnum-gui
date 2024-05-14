// window wrapper test

//#include "SDL.h"
#include "WindowWrapper.hh"

int main(){

    printf("window wrapper test\n");

    // create a shared pointer
    ShWindowWrapperPr window = WindowWrapper::create();

    printf("window ptr init\n");
    window->init();

    printf("window ptr close\n");
    window->close();

    //SDL_Quit();

}
