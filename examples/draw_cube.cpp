// gui testing
// cube

#include <Magnum/GL/AbstractFramebuffer.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/Shaders/VertexColorGL.h>
#include <Magnum/Shaders/Shaders.h>
#include "Magnum/Shaders/PhongGL.h"

#include <Magnum/Math/Matrix4.h>
#include <Magnum/Math/Color.h>
#include <Magnum/MeshTools/Compile.h>
#include "Magnum/Primitives/Cube.h"
#include "Magnum/Trade/MeshData.h"

#include "DrawCallback.hh"
#include "GuiBase.hh"
#include "log.hh"

using namespace smg;
using namespace Magnum;

// use data as void pointer in callback
class Data_ex {
public:
	std::string name = std::string("example");

	// mesh and shaders
	GL::Mesh _mesh;
	Shaders::PhongGL _shader;

	// 3D transformations
	Matrix4 _transformation;
	Matrix4 _projection;
	Color3 _color;
};
typedef std::shared_ptr<class Data_ex> ShDataPr;

int callback_fun(void* indata) {
	// example callback fun

	// cast our data to be meaningful
	// deference a cast to our shared pointer ...
	ShDataPr mydata = *static_cast<ShDataPr*>(indata);

	static bool hidden = false;

	ImGui::Begin("hey mom");
	ImGui::Text("my message is: %s", mydata->name.c_str());

	// check buttons
	if(ImGui::Button("hidden cube")) {
		hidden ^= true;
	}

	if(ImGui::Button("Another Window")) {
	}

	// hidden triangle
	if(hidden) {

		GL::defaultFramebuffer.clear(GL::FramebufferClear::Depth);
		mydata->_shader.setLightPositions({{1.4f, 1.0f, 0.75f, 0.0f}});
		mydata->_shader.setDiffuseColor(mydata->_color);
		mydata->_shader.setAmbientColor(Color3::fromHsv({mydata->_color.hue(), 1.0f, 0.0f}));
		mydata->_shader.setTransformationMatrix(mydata->_transformation);
		mydata->_shader.setNormalMatrix(mydata->_transformation.normalMatrix());
		mydata->_shader.setProjectionMatrix(mydata->_projection);
		mydata->_shader.draw(mydata->_mesh);
	}

	// text at end

	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
				1000.0 / Double(ImGui::GetIO().Framerate),
				Double(ImGui::GetIO().Framerate));
	ImGui::End();

	// 0 means success
	return 0;
}

int main(int argc, char** argv) {
	ShLogPr lg = Log::create();
	lg->msg("%sbegin main%s\n", KYEL, KNRM);

	// make our application class
	// this starts the gl context too
	lg->msg("make guibase application\n");
	GuiBase gui({argc, argv});

	// proper z order and discard triangles away from camera
	GL::Renderer::enable(GL::Renderer::Feature::DepthTest);
	GL::Renderer::enable(GL::Renderer::Feature::FaceCulling);

	// make the data structure
	ShDataPr mydata = std::make_shared<Data_ex>();
	mydata->name = "heymom";

	lg->msg("%smaking cube%s\n", KYEL, KNRM);

	// make our cube mesh and initial rotation
	mydata->_mesh = MeshTools::compile(Primitives::cubeSolid());
	mydata->_transformation = Matrix4::rotationX(30.0_degf) * Matrix4::rotationY(40.0_degf);

	// setup perspective projection
	mydata->_projection = Matrix4::perspectiveProjection(
							  35.0_degf, Vector2{gui.windowSize()}.aspectRatio(), 0.01f, 100.0f) *
						  Matrix4::translation(Vector3::zAxis(-10.0f));
	mydata->_color = Color3::fromHsv({35.0_degf, 1.0f, 1.0f});

	// make our call back
	ShDrawCallbackPr mycb = DrawCallback::create();
	mycb->set_callback(callback_fun);

	// reference the data here
	mycb->set_data((void*)&mydata);

	// set callback into our gui
	gui.add_callback(mycb);

	// exec calls mainloopiteration a bunch
	// this checks events and draws
	lg->msg("%sevent loop%s\n", KYEL, KNRM);
	bool done = false;
	while(!done) {
		done = !gui.mainLoopIteration();
	}

	// exit
	gui.exit();
}
