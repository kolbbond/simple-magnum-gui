#pragma once

#include <memory>
#include <Magnum/Magnum.h>
#include <Magnum/ImGuiIntegration/Integration.h>
#include <Magnum/Platform/Sdl2Application.h>

namespace guild {

typedef std::shared_ptr<class DrawCallback> ShDrawCallbackPr;

// for users to implement callbacks
// @hey, what else does user need to integrate?
typedef int (*draw_callback)(void*);

using namespace Magnum;

class DrawCallback {
protected:
	// draw callback function
	draw_callback callback_;

	// user data pointer
	void* data_ = nullptr;

	// flags for the supported events
	bool flag_viewport_event = false;
	bool flag_mousemove_event = false;
	bool flag_keypress_event = false;
	bool flag_keyrelease_event = false;
	bool flag_mousepress_event = false;
	bool flag_mousescroll_event = false;
	bool flag_textinput_event = false;

public:
	// constructor
	DrawCallback();

	// destructor
	~DrawCallback();

	// call the callback
	int call();

	// factory
	static ShDrawCallbackPr create();

	void* get_data();
	void set_callback(draw_callback);
	void set_data(void*);

	void viewportEvent(Magnum::Platform::Sdl2Application::ViewportEvent& event);
	void keyPressEvent(Magnum::Platform::Sdl2Application::KeyEvent& event);
	void keyReleaseEvent(Magnum::Platform::Sdl2Application::KeyEvent& event);
	void mousePressEvent(Magnum::Platform::Sdl2Application::MouseEvent& event);
	void mouseReleaseEvent(Magnum::Platform::Sdl2Application::MouseEvent& event);
	void mouseMoveEvent(Magnum::Platform::Sdl2Application::MouseMoveEvent& event);
	void mouseScrollEvent(Magnum::Platform::Sdl2Application::MouseScrollEvent& event);
	void textInputEvent(Magnum::Platform::Sdl2Application::TextInputEvent& event);
};
} // namespace guild
