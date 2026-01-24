// static helper functions
#pragma once

#include <memory>

#include <imgui.h>
#include <Magnum/ImGuiIntegration/Context.h>
#include <Magnum/GL/Mesh.h>

namespace smg {
typedef std::shared_ptr<class Statics> ShStaticsPr;
using namespace Magnum;

class Statics {

protected:

public:
	Statics();
	~Statics();

	static ShStaticsPr create();
	static void HelpMarker(const char* desc);
};

} // namespace smg
