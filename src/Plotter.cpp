// plotter overloads for Implot

#include "Plotter.hh"

namespace smg {

// plot like matlab
void Plotter::plot(arma::Col<double> x, arma::Col<double> y) {
	// wrap to below
	plot(arma::Row<double>(x.t()), arma::Row<double>(y.t()));
}

// adds a line plot to whatever the current ImPlot context is
void Plotter::plot_line(arma::Col<double> x, arma::Col<double> y) {
	// wrap to fundamental
	plot_line("temp", x, y);
}

void Plotter::plot_scatter(std::string str, arma::Col<double> x, arma::Col<double> y) {
	std::vector<double> xv = arma::conv_to<std::vector<double>>::from(x);
	std::vector<double> yv = arma::conv_to<std::vector<double>>::from(y);

	//ImPlot::PlotScatter("Points", &data[0].x, &data[0].y, data.size(), 0, 0, 2 * sizeof(double));
	ImPlot::PlotScatter(str.c_str(), xv.data(), yv.data(), yv.size());
}

void Plotter::plot_scatter(arma::Col<double> x, arma::Col<double> y) {
	// wrap to general
	plot_scatter("Points", x, y);
}

// adds a line plot to whatever the current ImPlot context is
void Plotter::plot_line(std::string str, arma::Col<double> x, arma::Col<double> y) {
	// check for column vectors @hey: add overloads for row vecs and matrices
	assert(x.n_cols == 1 && y.n_cols == 1);
	std::vector<double> xv = arma::conv_to<std::vector<double>>::from(x);
	std::vector<double> yv = arma::conv_to<std::vector<double>>::from(y);

	//printf("xv,yv size: (%lu,%lu): \n", xv.size(),yv.size());

	assert(xv.size() == yv.size());

	// add line
	ImPlot::PlotLine(str.c_str(), xv.data(), yv.data(), xv.size());
}

// plot
void Plotter::plot(arma::Row<double> x, arma::Row<double> y) {
	// convert to usable value by plotline
	std::vector<double> xv = arma::conv_to<std::vector<double>>::from(x);
	std::vector<double> yv = arma::conv_to<std::vector<double>>::from(y);

	// start the ImGui window,
	// we probably want to not open the window here
	//ImGui::Begin();
	ImPlot::BeginPlot("tester");

	// add line
	ImPlot::EndPlot();
}

void Plotter::set_axes_limits(arma::Col<double> x, arma::Col<double> y, double xbuf, double ybuf) {
	// set manual limits
	double minx, maxx, miny, maxy;
	miny = y.min() - ybuf;
	maxy = y.max() + ybuf;
	minx = x.min() - xbuf;
	maxx = x.max() + xbuf;

	// this call does assumes only the first call i.e. (setup)
	//	ImPlot::SetupAxesLimits(minx, maxx, miny, maxy);
	// use imgui flags to force limits
	Plotter::set_axes_limits(minx, maxx, miny, maxy);
}

void Plotter::set_axes_limits(double xmin, double xmax, double ymin, double ymax) {
	// this call does assumes only the first call i.e. (setup)
	//	ImPlot::SetupAxesLimits(minx, maxx, miny, maxy);
	// use imgui flags to force limits
	ImPlot::SetupAxesLimits(xmin, xmax, ymin, ymax, ImPlotCond_Always);
	//ImPlot::SetupAxesLimits(ImAxis_X1, xmin, xmax, ImGuiCond_Always);
	//ImPlot::SetupAxesLimits(ImAxis_Y1, ymin, ymax, ImGuiCond_Always);
}

void Plotter::set_axes_limits2(double xmin, double xmax, double ymin, double ymax) {
	// this call does assumes only the first call i.e. (setup)
	//	ImPlot::SetupAxesLimits(minx, maxx, miny, maxy);
	// use imgui flags to force limits
	ImPlot::SetupAxesLimits(xmin, xmax, ymin, ymax, ImPlotCond_Always);
	//ImPlot::SetupAxesLimits(ImAxis_X1, xmin, xmax, ImGuiCond_Always);
	//ImPlot::SetupAxesLimits(ImAxis_Y1, ymin, ymax, ImGuiCond_Always);
}

// save plot as image file?
// @hey, this can output a texture as png
// will be useful to output model pictures etc.
/*
void Plotter::save_texture_as_png(Magnum::GL::Texture2D texture, boost::filesystem::path fpath) {

	// instantiate the manager and converter
	PluginManager::Manager<Trade::AbstractImageConverter> manager;
	Containers::Pointer<Trade::AbstractImageConverter> converter = manager.loadAndInstantiate("AnyImageConverter");


	// create new image from texture
	Image2D image2{PixelFormat::RGBA8Unorm};
	mydata->texture.image(0, image2);

	if(!converter || !converter->convertToFile(image2, "image.png"))
		Fatal{} << "Can't save image.png with AnyImageConverter";
}
*/
} // namespace smg
