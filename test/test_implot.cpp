// implot testing
#include "GuiBase.hh"
#include "DrawCallback.hh"
#include "imgui.h"
#include "Plotter.hh"

#include <iostream>

// example data to pass into callback
class data_ex {
public:
	arma::Col<double> x;
	arma::Col<double> y;

	std::string name = "example";
};

using namespace smg;

int callback_fun(void* data) {
	// example callback fun

	// cast our data to be meaningful
	data_ex mydata = *reinterpret_cast<data_ex*>(data);

	ImGui::Begin("Plot Window");

	static bool show_plot;
	if(ImGui::Button("Test Window")) show_plot ^= true;
	if(show_plot) {
		if(ImPlot::BeginPlot("test")) {
			Plotter::plot_line(mydata.x, mydata.y);
			ImPlot::EndPlot();
		}
	}


	ImGui::End();

	// 0 means success
	return 0;
}

int main(int argc, char** argv) {

	// make our application class
	printf("make guibase application\n");
	GuiBase gui({argc, argv});

	// example data
	data_ex mydata = data_ex();
	mydata.x = arma::linspace(0, 1, 1000);
	mydata.y = arma::linspace(0, 1, 1000);
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
		done = !gui.mainLoopIteration();

		// done is true for the test
		done = true;
	}

	// exit
	gui.exit();
}
