// GuiMain.cpp
// 240420 -

// include header
#include "GuiMain.hh"
#include "WindowWrapper.hh"

// constructor
GuiMain::GuiMain() {
  // basic
}

int GuiMain::init() {
  // initialize everything
  // @hey check if set first???

  // success?
  return 1;
}

void GuiMain::handle_events() {
  // handle SDL stuff FIRST!

  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    ImGui_ImplSDL2_ProcessEvent(&event);
    if (event.type == SDL_QUIT)
      flag_done_ = true;
    if (event.type == SDL_WINDOWEVENT &&
        event.window.event == SDL_WINDOWEVENT_CLOSE &&
        event.window.windowID == SDL_GetWindowID(window_->get_window()))
      flag_done_ = true;
  }
}

void GuiMain::new_frame() {
  // start a new frame

  // Start the Dear ImGui frame
  // to allow other frame stepping perhaps
  imgui_->new_frame();
}

int GuiMain::demo_window() {
  // run a simple imgui demo

  // printf("run simple demo window\n");
  imgui_->demo_window();
  
  // we have to swap?
  window_->swap();

  // success?
  return 1;
}

int GuiMain::close() {
  // close everything
  window_->close();
  imgui_->close();
  implot_->close();

  // success
  return 1;
}

// SETTERS / setters

void GuiMain::set_window(ShWindowWrapperPr window) {
  // set window
  window_ = window;
}

void GuiMain::set_imgui(ShImGuiWrapperPr imgui) {
  // set imgui
  imgui_ = imgui;
}

void GuiMain::set_implot(ShImPlotWrapperPr implot) {
  // set implot
  implot_ = implot;
}

// GETTERS / getters
bool GuiMain::get_done() {
  // return
  return flag_done_;
}

// factory
ShGuiMainPr GuiMain::create() { return std::make_shared<GuiMain>(); }
