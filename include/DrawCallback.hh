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
typedef int (*mouse_move_event)(void*, Magnum::Platform::Sdl2Application::MouseMoveEvent& );
typedef int (*mouse_scroll_event)(void*, Magnum::Platform::Sdl2Application::MouseScrollEvent& );
typedef int (*key_press_event)(void*, Magnum::Platform::Sdl2Application::KeyEvent& );

using namespace Magnum;

class DrawCallback {
protected:
	// draw callback function
	draw_callback _callback;
	mouse_move_event _mouse_move_event;
	mouse_scroll_event _mouse_scroll_event;
	key_press_event _key_press_event;

	// user data pointer
	void* _data = nullptr;

	// flags for the supported events
	bool _flag_viewport_event = false;
	bool _flag_mouse_move_event = false;
	bool _flag_key_press_event = false;
	bool _flag_key_release_event = false;
	bool _flag_mouse_press_event = false;
	bool _flag_mouse_scroll_event = false;
	bool _flag_text_input_event = false;

public:
	// constructor
	DrawCallback();

	// destructor
	~DrawCallback();

	// call the callback
	int draw();

	// factory
	static ShDrawCallbackPr create();

	void* get_data();
	void set_callback(draw_callback);
	void set_data(void*);
	void set_mouse_move_event(mouse_move_event mme);
	void set_mouse_scroll_event(mouse_scroll_event mme);
	void set_key_press_event(key_press_event mme);

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
