// window wrapper test

//#include "SDL.h"
#include "WindowWrapper.hh"

int main(){

    printf("window wrapper test\n");
    //ShWindowWrapperPr window;
    WindowWrapper window;

    window.set_size(500,200);

    window.init();

    window.close();

    // must explicitly call SDL_Quit() in main
    //SDL_Quit();

}
