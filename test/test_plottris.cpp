
// test plotting tris

#include "GuiBase.hh"

// example data to pass into callback
struct data_ex {

	std::string name = "example";

	Magnum::Shaders::VertexColorGL2D shader;

	GL::Mesh mesh;
};


int callback_fun(void* data) {
	// example callback fun

	// cast our data to be meaningful
	data_ex* mydata = reinterpret_cast<data_ex*>(data);

	if(ImGui::Begin("hey mom")) {

		ImGui::Text("Hello, world!");
		if(ImGui::Button("Test Window")) {
		}
		if(ImGui::Button("Another Window")) {
		}
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
			1000.0 / Double(ImGui::GetIO().Framerate),

			Double(ImGui::GetIO().Framerate));

		// draw triangle
		mydata->shader.draw(mydata->mesh);

	} // imgui end
	ImGui::End();

	// 0 means success
	return 0;
}

using namespace smg;
int main(int argc, char** argv) {

	// make our application class
	printf("make guibase application\n");
	GuiBase gui({argc, argv});
	gui.setWindowSize(Vector2i{1920, 1080});
	gui.setWindowTitle("test_loadimage");

	// setup triangle example
	struct TriangleVertex {
		Vector2 position;
		Color3 color;
	};
	const TriangleVertex vertices[]{
		{{-0.5f, -0.5f}, 0xff0000_rgbf}, /* Left vertex, red color */
		{{0.5f, -0.5f}, 0x00ff00_rgbf}, /* Right vertex, green color */
		{{0.0f, 0.5f}, 0x0000ff_rgbf} /* Top vertex, blue color */
	};

	data_ex mydata;

	// create triangle mesh before game loop
	mydata.mesh.setCount(Containers::arraySize(vertices))
		.addVertexBuffer(
			GL::Buffer{vertices}, 0, Shaders::VertexColorGL2D::Position{}, Shaders::VertexColorGL2D::Color3{});

	// make our call back
	ShDrawCallbackPr mycb = DrawCallback::create();
	mycb->set_callback(callback_fun);

	// reference the data here
	mycb->set_data((void*)&mydata);

	// set callback into our gui
	gui.add_callback(mycb);

	bool done = false;
	while(!done) {

		done = !gui.mainLoopIteration();

		// done is true for the test
        // just to confirm the process works
        // turn off if you want to actually see the test
		done = true;
	}

	// exit
	gui.exit();
}
