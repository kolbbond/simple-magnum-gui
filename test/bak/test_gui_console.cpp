// test for gui console 

// #include <array>
#include <future>
#include <iostream>
#include <readline/readline.h>
#include <stdio.h>
#include <thread>

#include "GuiMain.hh"
#include "ImGuiWrapper.hh"
#include "ImPlotWrapper.hh"

static std::string get_userinput() {
  // user input function
  std::string str_in;
  std::getline(std::cin, str_in);
  // std::readline(std::cin, str_in);

  // check line?
  if (std::cin.eof() || str_in == "quit") {
    printf("see ya!\n");
  } else {
    printf("ECHO:\n");
  }

  return str_in;
}

int main(int argc, char *argv[]) {

  printf("in main\n");

  // test an array of Iops
  // change to vector to allow args (or maybe define a type ...?)

  //////////////////////////////////////////////////
  // GUI stuff here

  // create a shared pointer
  ShWindowWrapperPr window = WindowWrapper::create();
  window->init();

  ShImGuiWrapperPr imgui = ImGuiWrapper::create();
  imgui->init(window);

  ShImPlotWrapperPr implot = ImPlotWrapper::create();
  implot->init();

  ShGuiMainPr gui = GuiMain::create();
  // gui->init();

  gui->set_window(window);
  gui->set_imgui(imgui);
  gui->set_implot(implot);

  // reset current window
  //window->make_current();
  SDL_GL_MakeCurrent(window->get_window(), window->get_context());

  //////////////////////////////////////////////////
  // GAME LOOP

  // event loop
  bool done = false;
  while (!done) {

    // time to limit framerate
    int time_start = SDL_GetTicks();

    gui->handle_events();

    //////////////////////////////////////////////////
    // Throttle framerate
    // framerate
    int fps = 10000;
    int ms_per_frame = 1000 / fps;

    // this is dt
    int time = SDL_GetTicks() - time_start;
    if (time < 0)
      continue; // negative check

    // limit framerate here?
    // wait for the rest of dt
    int time_sleep = ms_per_frame - time;
    if (time_sleep > 0) {
      SDL_Delay(time_sleep);
    }

    gui->new_frame();

    //gui->demo_window();
    gui->demo_console();

    // render
    imgui->render();

    // debug done
    // auto set this for testing
    // done = true;
    done = gui->get_done();

  }

  printf("after game loop\n");

  // destroy everything
  gui->close();

  // get out of main
  return 0;
}
