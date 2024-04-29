// WindowWrapper.hh

#ifndef WindowWrapper_hh
#define WindowWrapper_hh
#define SDL_MAIN_HANDLED

// includes
// Using SDL, SDL OpenGL, GLEW
// need to check against the OS we are on
// #include <SDL.h>
// #include <SDL_opengl.h>

// general include
// @hey, change the cmake so it  finds the guild directory
#include "imgui.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"
#include "implot.h"
#include <SDL.h>
#include <SDL_opengl.h>


// c
#include <memory>
#include <stdio.h>
#include <string>

typedef std::shared_ptr<class WindowWrapper> ShWindowWrapperPr;

class WindowWrapper {

  // properties
protected:

    // window name
    // @hey, move to inherited class
    std::string name_ = "test window";

  // Screen dimension constants
  const int SCREEN_WIDTH_ = 640;
  const int SCREEN_HEIGHT_ = 480;

  // The window we'll be rendering to
  // SDL_Window* window_ = NULL;
  SDL_Window *window_;

  // window name
  // const char *name_ = "Vibecheck 0.000001";
  // std::string sname_ = "Vibecheck 0.000001";

  // OpenGL context
  SDL_GLContext glContext_;

  // Render flag
  // bool gRenderQuad_ = true;

  // Graphics program
  // GLuint gProgramID_ = 0;
  // GLint gVertexPos2DLocation_ = -1;
  // GLuint gVBO_ = 0;
  // GLuint gIBO_ = 0;

  // methods
public:
  // Constructor
  WindowWrapper();

  // factory
  static ShWindowWrapperPr create();

  // Starts up SDL, creates window, and initializes OpenGL
  bool init();

  // Initializes rendering program and clear color
  bool initGL();

  // Input handler
  void handleKeys(unsigned char key, int x, int y);

  // Per frame update
  void update();

  // Renders quad to the screen
  void render();

  // set the window name
  // determines the sdl context ... 
  void set_name(std::string);

  // get current ticks
  int get_ticks();

  // set this window as current
  void make_current();

  // swap buffers
  void swap();

  // sleeeep
  void sleep(int);

  // Frees media and shuts down SDL
  void close();

  // Shader loading utility programs
  void printProgramLog(GLuint program);
  void printShaderLog(GLuint shader);

  // get methods
  SDL_Window *get_window();
  SDL_GLContext get_context();
};

#endif
