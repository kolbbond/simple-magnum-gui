
// test creation of gui objects in a loop
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

// global exit bool
bool g_exit = false;
int callback_fun(void* data) {
	// example callback fun

	// cast our data to be meaningful
	data_ex mydata = *reinterpret_cast<data_ex*>(data);

	printf("debug callback\n");

	ImGui::Begin("hey mom");
	ImGui::Text("Hello, world!");
	if(ImGui::Button("Test Window")) {}
	if(ImGui::Button("Another Window")) {}
	if(ImGui::Button("Exit")) g_exit = true;
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0 / Double(ImGui::GetIO().Framerate), Double(ImGui::GetIO().Framerate));
	ImGui::End();

	// 0 means success
	return 0;
}

int main(int argc, char** argv) {

	// how many times to recreate the gui
	int num_loops = 3;

	// walk over loops
	for(int i = 0; i < num_loops; i++) {
		// make our application class
		printf("make guibase application\n");
		GuiBase gui({ argc, argv });

		// example data
		data_ex mydata = data_ex();
		mydata.x = 5;
		mydata.y = 6;
		mydata.name = "heymom";

		// make our call back
		ShDrawCallbackPr mycb = DrawCallback::create();
		mycb->set_callback(callback_fun);

		// reference the data here
		mycb->set_data((void*)&mydata);

		// set callback into our gui
		gui.add_callback(mycb);

		// exec calls mainloopiteration a bunch
		// this checks events and draws
		std::string input;
		//std::cin >> input;
		bool done = false;
		while(!done) {
			printf("loop iteration\n");

			// to allow override of test with -g flag
			if(argc == 2 && strcmp(argv[1], "-g") == 0) {
				done = false;
			} else {
				done = true;
				break;
			}

			// check global
			if(g_exit) {
				done = true;
				break;
			}
			// done is true for the test
			//done = true;

			// check gui exit
			done = !gui.mainLoopIteration();
		}

		// exit
		gui.exit();

		// reset global exit flag
		g_exit = false;
	}


	// done
}
