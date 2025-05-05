// implot testing
#include "GuiBase.hh"
#include "DrawCallback.hh"
#include "imgui.h"
#include "Plotter.hh"

#include "implot3d.h"

#include <iostream>

// example data to pass into callback
class data_ex {
public:
	arma::Col<double> x;
	arma::Col<double> y;
	arma::Col<double> z;

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
		if(ImPlot3D::BeginPlot("test")) {
			//Plotter::plot_line(mydata.x, mydata.y);
			std::vector<double> xv = arma::conv_to<std::vector<double>>::from(mydata.x);
			std::vector<double> yv = arma::conv_to<std::vector<double>>::from(mydata.y);
			std::vector<double> zv = arma::conv_to<std::vector<double>>::from(mydata.z);
			assert(xv.size() == yv.size());
			assert(xv.size() == zv.size());

			// map to vector
			ImPlot3D::PlotScatter("test", xv.data(), yv.data(), zv.data(), xv.size());
			ImPlot3D::EndPlot();
		}
	}


	ImGui::End();

	// 0 means success
	return 0;
}

int main(int argc, char** argv) {

	// make our application class
	printf("make guibase application\n");
	GuiBase gui({ argc, argv });

	// example data
	arma::Col<double> t = arma::linspace(0, 1, 1000);
	double R = 0.6; // [m]
	double pitch = 30e-3; // [m]
	data_ex mydata = data_ex();
	mydata.x = R * arma::cos(2 * M_PI * t);
	mydata.y = R * arma::sin(2 * M_PI * t);
	mydata.z = pitch * t;
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

		// check for gui to allow manual override
		if(argc == 2 && strcmp(argv[1], "-g") == 0) {
			done = false;
		} else {
			done = true;
		}
	}

	// exit
	gui.exit();
}
