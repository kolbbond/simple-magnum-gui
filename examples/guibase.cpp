// implot testing
#include "GuiBase.hh"
#include "DrawCallback.hh"
#include "imgui.h"

#include <iostream>

// example data to pass into callback
class data_ex {
public:
	int x;
	int y;

	std::string name = "example";
};

using namespace smg;

int callback_fun(void* data) {
	// example callback fun

	// cast our data to be meaningful
	data_ex mydata = *reinterpret_cast<data_ex*>(data);
	(void)mydata; // suppress unused warning

	ImGui::Begin("hey mom");
	ImGui::Text("Hello, world!");
	if(ImGui::Button("Test Window")) {
	}
	if(ImGui::Button("Another Window")) {
	}
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
		1000.0 / Double(ImGui::GetIO().Framerate),
		Double(ImGui::GetIO().Framerate));
	ImGui::End();

	// 0 means success
	return 0;
}

// Custom application class that sets up callback in constructor
class GuiBaseExample : public GuiBase {
public:
	explicit GuiBaseExample(const Arguments& arguments)
		: GuiBase(arguments) {
		// example data
		_mydata.x = 5;
		_mydata.y = 6;
		_mydata.name = "heymom";

		// make our callback
		_mycb = DrawCallback::create();
		_mycb->set_callback(callback_fun);
		_mycb->set_data((void*)&_mydata);

		// set callback into our gui
		add_callback(_mycb);
	}

private:
	data_ex _mydata;
	ShDrawCallbackPr _mycb;
};

// Use MAGNUM_APPLICATION_MAIN for proper cross-platform main loop
// This handles Emscripten's event loop correctly
MAGNUM_APPLICATION_MAIN(GuiBaseExample)
