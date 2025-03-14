// MVP bug example
// some error in tests when running gui
// No OpenGL context when exiting
//
// Issue is using static OpenGL context objects

// implot testing
#include "DrawCallback.hh"
#include "GuiBase.hh"
#include "Magnum/Math/Matrix4.h"
#include "Plotter.hh"
#include "imgui.h"

#include <iostream>

void cube_example(GL::Mesh &m) {

  // Cube vertices (position only)
  struct Vertex {
    Vector3 position;
  };
  const Vertex vertices[]{
      {{-1.0f, -1.0f, 1.0f}},  // 0
      {{1.0f, -1.0f, 1.0f}},   // 1
      {{1.0f, 1.0f, 1.0f}},    // 2
      {{-1.0f, 1.0f, 1.0f}},   // 3
      {{-1.0f, -1.0f, -1.0f}}, // 4
      {{1.0f, -1.0f, -1.0f}},  // 5
      {{1.0f, 1.0f, -1.0f}},   // 6
      {{-1.0f, 1.0f, -1.0f}}   // 7
  };

  // Cube indices (two triangles per face)
  // clang-format off
	const UnsignedInt indices[]{
		0,		1,		2,		2,		3,		0, // Front}
        1,		5,		6,		6,		2,		1, // Right
		5,		4,		7,		7,		6,		5, // Back
		4,		0,		3,		3,		7,		4, // Left
		3,		2,		6,		6,		7,		3, // Top
		4,		5,		1,		1,		0,		4 // Bottom
	};
  // clang-format on

  GL::Buffer vertexBuffer, indexBuffer;

  vertexBuffer.setData(vertices);
  indexBuffer.setData(indices);

  // create here
  m = GL::Mesh{};
  m.setPrimitive(GL::MeshPrimitive::Triangles);
  m.setCount(36)
      .addVertexBuffer(std::move(vertexBuffer), 0,
                       Shaders::FlatGL3D::Position{})
      .setIndexBuffer(std::move(indexBuffer), 0,
                      GL::MeshIndexType::UnsignedInt);

  // return m;
}

// global shader

Shaders::FlatGL3D g_shader{NoCreate};

// example data to pass into callback
class data_ex {
public:
  arma::Col<double> x;
  arma::Col<double> y;

  std::string name = "example";
  GL::Mesh mesh{NoCreate};
};

using namespace smg;

int callback_fun(void *data) {
  // check context
  if (!Magnum::GL::Context::hasCurrent()) {
    Debug{} << "Skipping shader draw: No OpenGL context";
    return 1;
  }

  // example callback fun

  // cast our data to be meaningful
  data_ex *mydata = reinterpret_cast<data_ex *>(data);

  ImGui::Begin("Plot Window");

  static bool show_plot;
  if (ImGui::Button("Test Window"))
    show_plot ^= true;
  if (show_plot) {
    if (ImPlot::BeginPlot("test")) {
      Plotter::plot_line(mydata->x, mydata->y);
      ImPlot::EndPlot();
    }
  }

  static GL::Mesh &mesh = mydata->mesh;
  Matrix4 transformationMatrix = Matrix4::translation(Vector3::zAxis(-10.00f)) *
                                 Matrix4::rotationX(Deg(35.0f)) *
                                 Matrix4::rotationY(Deg(35.0f));
  Matrix4 projectionMatrix =
      Matrix4::perspectiveProjection(Deg(35.0f), 1.0f, 0.1f, 100.0f);
  g_shader.setTransformationProjectionMatrix(projectionMatrix *
                                             transformationMatrix);
  g_shader.setColor(Color3(1.0f, 0.0f, 1.0f));
  GL::Renderer::disable(GL::Renderer::Feature::FaceCulling);
  GL::Renderer::disable(GL::Renderer::Feature::DepthTest);
  g_shader.draw(mydata->mesh);

  ImGui::End();

  // 0 means success
  return 0;
}

// main
int main(int argc, char **argv) {

  // make our application class
  printf("make guibase application\n");
  GuiBase gui({argc, argv});
  gui.set_window_size(1920, 1080);
  g_shader = Shaders::FlatGL3D{};

  // example data
  data_ex mydata;
  mydata.x = arma::linspace(0, 1, 1000);
  mydata.y = arma::linspace(0, 1, 1000);
  mydata.name = "heymom";

  // make our call back
  ShDrawCallbackPr mycb = DrawCallback::create();
  mycb->set_callback(callback_fun);

  // create mesh
  cube_example(mydata.mesh);

  // reference the data here
  mycb->set_data((void *)&mydata);

  // set callback into our gui
  gui.add_callback(mycb);

  // exec calls mainloopiteration a bunch
  // this checks events and draws
  std::string input;
  // std::cin >> input;
  bool done = false;
  while (!done) {
    done = !gui.mainLoopIteration();

    // done is true for the test
    done = true;
  }

  // check context
  if (!Magnum::GL::Context::hasCurrent()) {
    Debug{} << "No OpenGL context available! Mesh destruction aborted.";
  }

  // destroy mesh
  mydata.mesh = GL::Mesh{NoCreate};
  g_shader = Shaders::FlatGL3D{NoCreate};

  // exit
  gui.exit();

  // check context
  if (!Magnum::GL::Context::hasCurrent()) {
    Debug{} << "No OpenGL context available! Mesh destruction aborted.";
  }
}
