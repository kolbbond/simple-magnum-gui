#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Platform/Sdl2Application.h>
#include <Magnum/Shaders/Shaders.h>
#include <Magnum/Shaders/VertexColorGL.h>

// raw test of libraries

using namespace Magnum;

class TriangleEx : public Platform::Application {
public:
  explicit TriangleEx(const Arguments &arguments);

private:
  void drawEvent() override;

  GL::Mesh mesh_;
  Shaders::VertexColorGL2D shader_;
};

TriangleEx::TriangleEx(const Arguments &arguments)
    : Platform::Application{
          arguments, Configuration{}.setTitle("Magnum Triangle Example")} {
  using namespace Math::Literals;

  struct TriangleVertex {
    Vector2 position;
    Color3 color;
  };
  const TriangleVertex vertices[]{
      {{-0.5f, -0.5f}, 0xff0000_rgbf}, /* Left vertex, red color */
      {{0.5f, -0.5f}, 0x00ff00_rgbf},  /* Right vertex, green color */
      {{0.0f, 0.5f}, 0x0000ff_rgbf}    /* Top vertex, blue color */
  };
  mesh_.setCount(Containers::arraySize(vertices))
      .addVertexBuffer(GL::Buffer{vertices}, 0,
                       Shaders::VertexColorGL2D::Position{},
                       Shaders::VertexColorGL2D::Color3{});
}

void TriangleEx::drawEvent() {
  GL::defaultFramebuffer.clear(GL::FramebufferClear::Color);

  shader_.draw(mesh_);

  swapBuffers();
}
