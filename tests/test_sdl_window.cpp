/*This source code copyrighted by Lazy Foo' Productions 2004-2024
and may not be redistributed without written permission.*/

// our includes
#include "WindowWrapper.hh"

// standards
#include <stdio.h>
#include <string>

int main(int argc, char *args[]) {

  // SDL straight
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) !=
      0) {
    printf("Error: %s\n", SDL_GetError());
    return -1;
  }

  // GL 3.0 + GLSL 130
  // const char *glsl_version = "#version 130";
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

  // Create window with graphics context
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  SDL_WindowFlags window_flags =
      (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                        SDL_WINDOW_ALLOW_HIGHDPI);

  // create our window
  SDL_Window *window_ =
      SDL_CreateWindow("plot example", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
  if (window_ == nullptr) {
    printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
    return -1;
  }

  // context
  printf("creating context\n");
  SDL_GLContext glContext_ = SDL_GL_CreateContext(window_);
  SDL_GL_MakeCurrent(window_, glContext_);
  SDL_GL_SetSwapInterval(1); // Enable vsync

  printf("successful init\n");

  // destroy

  printf("destroying window\n");

  // destroy windows
  SDL_GL_DeleteContext(glContext_);
  SDL_DestroyWindow(window_);
  SDL_Quit();

  printf("after SDL quit\n");
  return 0;

}
