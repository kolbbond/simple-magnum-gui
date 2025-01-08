// static helper functions
#pragma once

#include <memory>
#include <armadillo>

#include "MagnumExternal/ImGui/imgui.h"
#include "Magnum/ImGuiIntegration/Context.h"

#include "typedefs.hh"

namespace smg {
typedef std::shared_ptr<class Statics> ShStaticsPr;
using namespace Magnum;

class Statics {

	// properties
protected:
	// methods
public:
	// constructor
	Statics();

	// destructor
	~Statics();

	// factory
	static ShStaticsPr create();
	static void HelpMarker(const char* desc);

	// conversions
	static Vector3 conv2Vector3(arma::Col<fltp> Rn);
	static arma::Col<fltp> conv2arma(Vector3 Rn);
};

} // namespace smg
