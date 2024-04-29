// ImGuiWrapper.hh

#ifndef ImGuiWrapper_hh
#define ImGuiWrapper_hh

#include "WindowWrapper.hh"

typedef std::shared_ptr<class ImGuiWrapper> ShImGuiWrapperPr;
class ImGuiWrapper {

  // properties
protected:
  // input/output
  ImGuiIO io_;

  // when refreshed
  ImVec4 clear_color_ = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  // methods
public:
  ImGuiWrapper();
  int init(ShWindowWrapperPr);
  int close();

  void new_frame();

  int demo_window();

  void render();

  // factory
  static ShImGuiWrapperPr create();
};

#endif
