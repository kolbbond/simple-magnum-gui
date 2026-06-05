// main gui class
#pragma once

#include <tuple>
#include <vector>

// magnum includes
#include <Corrade/configure.h>
#include <Magnum/Magnum.h>

// Platform-specific application
#if defined(CORRADE_TARGET_EMSCRIPTEN)
#include <Magnum/Platform/EmscriptenApplication.h>
#else
#include <Magnum/Platform/Sdl2Application.h>
#include "SDL_video.h"
#endif

#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/ImGuiIntegration/Context.hpp>
#include <Magnum/Math/Color.h>
#include <Magnum/Shaders/VertexColorGL.h>
#include <Magnum/Image.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/Trade.h>

#include "log.hh"

#include "imgui.h"
#include "log.hh"

// smg includes
#include "DrawCallback.hh"
#include "implot.h"

namespace smg {

// base gui class, entry point for guis

class GuiBase: public Magnum::Platform::Application {

protected:
	// our imgui context
	Magnum::ImGuiIntegration::Context _imgui{ Magnum::NoCreate };

#if !defined(CORRADE_TARGET_EMSCRIPTEN)
	// actual window (desktop only, assume SDL); non-owning, owned by Platform::Application
	SDL_Window* _window = nullptr;
#endif

	// logger (safe no-op default until the constructor installs a real Log)
	ShLogPr _lg = NullLog::create();

	bool _showDemoWindow = true;
	bool _showAnotherWindow = false;
	Magnum::Color4 _clearColor; // initialized in constructor
	Magnum::Float _floatValue = 0.0f;

	int _samples = 4; // MSAA samples

	// font setting
	std::vector<Corrade::Containers::ArrayView<const char>> _fontData;
	std::vector<ImFont*> _fonts;
	ImFont* _font_default = nullptr;

	// icon settings
	Corrade::Containers::Optional<Magnum::Trade::ImageData2D> _icon;

	// list of set callbacks
	std::vector<ShDrawCallbackPr> _callback_list;

public:
	// constructor
	explicit GuiBase(const Arguments& arguments);

	~GuiBase() {
		//	std::printf(" [X] GuiBase destructor [X] \n");
		this->exit();
	};

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

	// getters (some are desktop-only)
	std::pair<int, int> get_window_position();
#if !defined(CORRADE_TARGET_EMSCRIPTEN)
	SDL_Window* get_window();
	void set_window_icon(std::string icon_file);
	void set_window_position(int x, int y);
	void set_window_size(int x, int y);
#endif

	// event wrappers
	void viewportEvent(ViewportEvent& event) override;
	void keyPressEvent(KeyEvent& event) override;
	void keyReleaseEvent(KeyEvent& event) override;
	void pointerPressEvent(PointerEvent& event) override;
	void pointerReleaseEvent(PointerEvent& event) override;
	void pointerMoveEvent(PointerMoveEvent& event) override;
	void scrollEvent(ScrollEvent& event) override;
	void textInputEvent(TextInputEvent& event) override;
};
} // namespace smg
