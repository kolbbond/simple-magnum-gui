

#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/ImGuiIntegration/Context.hpp>
#include <Magnum/Math/Color.h>

#ifdef CORRADE_TARGET_ANDROID
#include <Magnum/Platform/AndroidApplication.h>
#elif defined(CORRADE_TARGET_EMSCRIPTEN)
#include <Magnum/Platform/EmscriptenApplication.h>
#else
#include <Magnum/Platform/Sdl2Application.h>
#endif

using namespace Magnum::Math::Literals;
using namespace Magnum;

// using namespace Magnum::Math::Literals;
class ImGuiExample2 : public Magnum::Platform::Application {
private:
  Magnum::ImGuiIntegration::Context _imgui{NoCreate};

  bool _showDemoWindow = true;
  bool _showAnotherWindow = false;
  Color4 _clearColor = 0x72909aff_rgbaf;
  Float _floatValue = 0.0f;

public:
  explicit ImGuiExample2(const Arguments &arguments);

  void drawEvent() override;

  void viewportEvent(ViewportEvent &event) override;

  void keyPressEvent(KeyEvent &event) override;
  void keyReleaseEvent(KeyEvent &event) override;

  void mousePressEvent(MouseEvent &event) override;
  void mouseReleaseEvent(MouseEvent &event) override;
  void mouseMoveEvent(MouseMoveEvent &event) override;
  void mouseScrollEvent(MouseScrollEvent &event) override;
  void textInputEvent(TextInputEvent &event) override;
};
