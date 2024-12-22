// ImGuiWrapper.cpp
// 240219 -

// include header
#include "ImGuiWrapper.hh"
#include "WindowWrapper.hh"
#define IM_TRACE_LOCATION()                                                    \
  if (ImGui::Begin("Function Trace")) {                                        \
    ImGui::Text("%s(), %s::%d", __FUNCTION__, __FILE__, __LINE__);             \
  }                                                                            \
  ImGui::End();

// constructor
ImGuiWrapper::ImGuiWrapper() {}

int ImGuiWrapper::init(ShWindowWrapperPr gui) {
  // initialize ImGui

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  printf("creating ImGui context\n");
  ImGui::CreateContext();

  // input/output stuff
  io_ = ImGui::GetIO();
  //(void)io_;
  io_.ConfigFlags |=
      ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
  io_.ConfigFlags |=
      ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls

  // color
  // ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  // GL 3.0 + GLSL 130
  // @hey, move to gui???
  const char *glsl_version = "#version 130";

  // Setup Dear ImGui style
  // ImGui::StyleColorsDark();
  ImGui::StyleColorsLight();
  // Setup Platform/Renderer backends
  //ImGui_ImplSDL2_InitForOpenGL(gui->get_window(), gui->get_context());
  //ImGui_ImplOpenGL3_Init(glsl_version);

  // IMPLOT
  // implotting here
  printf("creating ImGui context\n");
  //ImPlot::CreateContext();

  // success?
  return 1;
}

void ImGuiWrapper::new_frame() {
  // Start the Dear ImGui frame
  //ImGui_ImplOpenGL3_NewFrame();
  //ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();
}

int ImGuiWrapper::demo_window() {
  // demo of a basic window

  // demo window options
  bool show_demo_window = true;
  bool show_debug_window = true;

  if (show_demo_window) {
    ImGui::ShowDemoWindow(&show_demo_window);
  }
  if (show_debug_window) {
    ImGui::ShowMetricsWindow(&show_debug_window);
    ImGui::ShowDebugLogWindow(&show_debug_window);
    ImGui::ShowIDStackToolWindow(&show_debug_window);
  }

  {
    // printf("demo_window\n");
    static float f = 0.0f;
    static int counter = 0;

    // this is our own window
    ImGui::Begin("Main Window"); // Create a window called "Hello, world!"
                                 // and append into it.

    ImGui::Text("custom text."); // Display some text (you can
                                 // use a format strings too)
    ImGui::Checkbox(
        "Demo demo",
        &show_demo_window); // Edit bools storing our window open/close state

    // Edit 1 float using a slider from 0.0f to 1.0f
    ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
    ImGui::ColorEdit3(
        "clear color",
        (float *)&clear_color_); // Edit 3 floats representing a color

    // button counter
    if (ImGui::Button("Button"))
      counter++;

    ImGui::SameLine();
    ImGui::Text("counter = %d", counter);

    ImGui::Text("Application average fps %.3f ms/frame (%.1f FPS)",
                1000.0f / io_.Framerate, io_.Framerate);
    // debug
    printf("fps: %0.2f\n",io_.Framerate);
    if (ImGui::BeginMenu("options")) {
      // IMGUI_DEMO_MARKER("Menu/Examples");
      // ImGui::MenuItem("Main menu bar", NULL, &show_app_main_menu_bar);
      ImGui::MenuItem("Main Menu");
      ImGui::SeparatorText("Mini apps");
      // ImGui::MenuItem("Console", NULL, &show_app_console);
      IM_TRACE_LOCATION();
      ImGui::MenuItem("Console");
      ImGui::EndMenu();
    }
    ImGui::End();
  }

  // implot demo
  ImGui::Begin("implot demo");
  //ImPlot::ShowDemoWindow();
  ImGui::End();

  // success?
  return 1;
}
void ImGuiWrapper::render() {
  // render all imgui sets
  // printf("rendering\n");
  ImGui::Render();
  glViewport(0, 0, (int)io_.DisplaySize.x, (int)io_.DisplaySize.y);
  glClearColor(clear_color_.x * clear_color_.w, clear_color_.y * clear_color_.w,
               clear_color_.z * clear_color_.w, clear_color_.w);
  glClear(GL_COLOR_BUFFER_BIT);
  //ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

int ImGuiWrapper::close() {
  printf("annihilate context/window\n");
  //ImPlot::DestroyContext();
  //ImGui_ImplOpenGL3_Shutdown();
  //ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();

  // success?
  return 1;
}

// factory
ShImGuiWrapperPr ImGuiWrapper::create() {
  return std::make_shared<ImGuiWrapper>();
}
