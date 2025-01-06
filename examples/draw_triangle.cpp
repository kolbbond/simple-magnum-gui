// implot testing

#include <Magnum/Shaders/Shaders.h>
#include <iostream>
#include <memory>

#include "Magnum/Shaders/VertexColorGL.h"
#include "imgui.h"

#include "DrawCallback.hh"
#include "GuiBase.hh"
#include "log.hh"

using namespace smg;
using namespace Magnum;

// type of geometry
struct TriangleVertex {
	Vector2 position;
	Color3 color;
};

// use data as void pointer in callback
class Data_ex {
public:
	int x;
	int y;

	std::string name = std::string("example");
	char myname[50] = "heymom";

	GL::Mesh mesh;
	Shaders::VertexColorGL2D shader;
	TriangleVertex vertices[3]{
		{{-0.5f, -0.5f}, 0xff0000_rgbf}, /* Left vertex, red color */
		{{0.5f, -0.5f}, 0x00ff00_rgbf}, /* Right vertex, green color */
		{{0.0f, 0.5f}, 0x0000ff_rgbf} /* Top vertex, blue color */
	};
};
typedef std::shared_ptr<class Data_ex> ShDataPr;

int callback_fun(void* indata) {
	// example callback fun

    // cast our void pointer to relevant data
	ShDataPr mydata = *static_cast<ShDataPr*>(indata);

	static bool hidden = false;

	ImGui::Begin("hey mom");
	ImGui::Text("my message is: %s", mydata->name.c_str());

	// check buttons
	if(ImGui::Button("hidden triangle")) {
		hidden ^= true;
	}

	if(ImGui::Button("Another Window")) {
	}

	// hidden triangle
	if(hidden) {

		mydata->mesh.setCount(Containers::arraySize(mydata->vertices))
			.addVertexBuffer(GL::Buffer{mydata->vertices},
							 0,
							 Shaders::VertexColorGL2D::Position{},
							 Shaders::VertexColorGL2D::Color3{});

		mydata->shader.draw(mydata->mesh);
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
	lg->msg("make guibase application\n");
	GuiBase gui({argc, argv});

	// make the triangle
	ShDataPr mydata = std::make_shared<Data_ex>();
	mydata->name = "heymom";

	lg->msg("%smaking triangles%s\n", KYEL, KNRM);

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
