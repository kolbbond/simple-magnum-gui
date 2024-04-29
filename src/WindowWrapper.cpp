// WindowWrapper.cpp
// 240219 -

// include header
#include "WindowWrapper.hh"

// constructor
WindowWrapper::WindowWrapper() {}

bool WindowWrapper::init() {
  // create the window (SDL2 for now)

  // Initialization flag
  bool success = true;

  // Setup SDL
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) !=
      0) {
    printf("Error: %s\n", SDL_GetError());
    return -1;
  }
  // atexit(SDL_Quit());

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
  window_ = SDL_CreateWindow(name_.c_str(), SDL_WINDOWPOS_CENTERED,
                             SDL_WINDOWPOS_CENTERED, 800, 600, window_flags);
  if (window_ == nullptr) {
    printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
    return -1;
  }

  // context
  printf("creating context\n");
  glContext_ = SDL_GL_CreateContext(this->window_);
  SDL_GL_MakeCurrent(window_, glContext_);
  SDL_GL_SetSwapInterval(1); // Enable vsync

  printf("successful init\n");
  return success;
}

bool WindowWrapper::initGL() {
  // Success flag
  bool success = true;

  return success;
}

void WindowWrapper::handleKeys(unsigned char key, int x, int y) {
  // Toggle quad
  if (key == 'q') {
    // gRenderQuad_ = !gRenderQuad_;
  }

  // what are we doing with x and y
  x = y;
  y = x;
}

void WindowWrapper::update() {
  // No per frame update needed
}

void WindowWrapper::render() {
  // Clear color buffer
}

void WindowWrapper::close() {
  // Deallocate program
  // glDeleteProgram(gProgramID_);

  printf("destroying window\n");

  // destroy windows
  SDL_GL_DeleteContext(this->glContext_);
  SDL_DestroyWindow(this->window_);

  // don't call SDL_Quit inside an object
  SDL_Quit();

  printf("after SDL quit\n");
}

void WindowWrapper::make_current(){
    // make this window current
  SDL_GL_MakeCurrent(window_, glContext_);
}
void WindowWrapper::swap() {
  // swap the buffer
  SDL_GL_SwapWindow(window_);
}

// SETTERS / setters
void WindowWrapper::set_name(std::string name) {
  // set name
  name_ = name;
}

// GETTERS
int WindowWrapper::get_ticks() {
  // get the current tick
  return SDL_GetTicks();
}
SDL_Window *WindowWrapper::get_window() {
  // return window
  return window_;
}

SDL_GLContext WindowWrapper::get_context() {
  // return context
  return glContext_;
}

// SETTERS
// PRINTERS

/*
void WindowWrapper::printProgramLog(GLuint program) {
  // Make sure name is shader
    //program = NULL;
}

void WindowWrapper::printShaderLog(GLuint shader) {
  // Make sure name is shader
}
*/

// factory
ShWindowWrapperPr WindowWrapper::create() {
  return std::make_shared<WindowWrapper>();
}
