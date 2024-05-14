// test of magnum integration example

#include "ImGuiExample.hh"
#include <Corrade/Utility/Utility.h>

using namespace Magnum;
using namespace Magnum::Platform;
using namespace Magnum::Platform;

int main(int argv, char** argc) {

  // Magnum::ImGuiIntegration::Context imgui = ImGuiIntegration::Context(
  //    Vector2{windowSize()} / dpiScaling(), windowSize(), framebufferSize());

  //Application::Arguments args = Application::Arguments();

  ImGuiExample gui = ImGuiExample({argv,argc});
}
