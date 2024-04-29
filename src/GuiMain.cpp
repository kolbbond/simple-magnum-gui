// GuiMain.cpp
// 240420 -

// include header
#include "GuiMain.hh"
#include "GuiConsole.hh"
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

int GuiMain::demo_console() {
  // make a demo console

  // test window
  // ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(500, 800));
  ImGui::Begin("console");
  ImGui::End();

  ShGuiConsolePr gc = GuiConsole::create();
  bool flag_console = true;
  gc->ShowGuiConsole(&flag_console);

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
ShGuiMainPr GuiMain::create() {
  // factory
  return std::make_shared<GuiMain>();
}
