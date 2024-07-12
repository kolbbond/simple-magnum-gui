// main gui class

// magnum includes
#include <Magnum/Platform/Sdl2Application.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/ImGuiIntegration/Context.hpp>
#include <Magnum/Math/Color.h>

#include <tuple>
#include <vector>
#include "log.hh"

#include "SDL_video.h"
#include "imgui.h"
#include "log.hh"
#include "typedefs.hh"
#include <Magnum/Magnum.h>

// guild includes
#include "DrawCallback.hh"
#include "implot.h"
#include "common.hh"

using namespace Magnum;
using namespace Math::Literals;

namespace guild {

// base gui class, entry point for guis

class GuiBase: public Platform::Application {

protected:
	// our imgui context
	ImGuiIntegration::Context _imgui{NoCreate};

	// actual window (assume SDL?)
	SDL_Window* _window;

	// logger
	ShLoggerPr _lg; // = NullLogger::create();

	bool _showDemoWindow = true;
	bool _showAnotherWindow = false;
	Color4 _clearColor = 0x72909aff_rgbaf;
	Float _floatValue = 0.0f;

	// list of set callbacks
	std::vector<ShDrawCallbackPr> _callback_list;

public:
	// constructor
	explicit GuiBase(const Arguments& arguments);

	// draw callbacks
	// main draw event loop (called every iteration)
	void drawEvent() override;
	void drawBegin();
	void drawEnd();
	void draw_callbacks();

	// demo
	void demo_imgui();

	// implot demo
	void demo_implot();
	void demo_test();

	// printers
	void print_window_position();

	// add custom callbacks
	void add_callback(ShDrawCallbackPr);

	// getters
	SDL_Window* get_window();
	std::pair<int, int> get_window_position();
	void set_window_position(int x, int y);
	void set_window_size(int x, int y);

	// event wrappers
	void viewportEvent(ViewportEvent& event) override;
	void keyPressEvent(KeyEvent& event) override;
	void keyReleaseEvent(KeyEvent& event) override;
	void mousePressEvent(MouseEvent& event) override;
	void mouseReleaseEvent(MouseEvent& event) override;
	void mouseMoveEvent(MouseMoveEvent& event) override;
	void mouseScrollEvent(MouseScrollEvent& event) override;
	void textInputEvent(TextInputEvent& event) override;
};
} // namespace guild
