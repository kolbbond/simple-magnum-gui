// plotter.hh header
#pragma once

#include "implot.h"

// @hey: need to define this out?
#include <armadillo>

namespace smg {

class Plotter {

public:
	// like plot
	static void plot(arma::Col<double> x, arma::Col<double> y);
	static void plot(arma::Row<double> x, arma::Row<double> y);

	static void plot_line(arma::Col<double> x, arma::Col<double> y);
	static void plot_line(std::string str, arma::Col<double> x, arma::Col<double> y);

	static void set_axes_limits(arma::Col<double> x, arma::Col<double> y, double xbuf, double ybuf);
	static void set_axes_limits(double xmin, double xmax, double ymin, double ymax);
	static void set_axes_limits2(double xmin, double xmax, double ymin, double ymax);

	static void plot_scatter(std::string str, arma::Col<double> x, arma::Col<double> y);
	static void plot_scatter(arma::Col<double> x, arma::Col<double> y);
	static void plot_scatter(double x, double y);
};

} // namespace smg
