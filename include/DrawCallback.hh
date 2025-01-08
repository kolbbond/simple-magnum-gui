#pragma once

#include <memory>
#include <Magnum/Magnum.h>
#include <Magnum/ImGuiIntegration/Integration.h>
#include <Magnum/Platform/Sdl2Application.h>

namespace smg {

typedef std::shared_ptr<class DrawCallback> ShDrawCallbackPr;

// for users to implement callbacks
// @hey: what else does user need to integrate?
// @hey: also these are deprecated
typedef int (*draw_callback)(void*);
//typedef int (*pointer_move_event)(void*, Magnum::Platform::Sdl2Application::MouseMoveEvent& );
//typedef int (*pointer_scroll_event)(void*, Magnum::Platform::Sdl2Application::MouseScrollEvent& );
//typedef int (*key_press_event)(void*, Magnum::Platform::Sdl2Application::KeyEvent& );
typedef int (*pointer_move_event)(void*, Magnum::Platform::Sdl2Application::PointerMoveEvent& );
typedef int (*scroll_event)(void*, Magnum::Platform::Sdl2Application::ScrollEvent& );
typedef int (*key_press_event)(void*, Magnum::Platform::Sdl2Application::KeyEvent& );

using namespace Magnum;

class DrawCallback {
protected:
	// draw callback function
	draw_callback _callback;
	pointer_move_event _pointer_move_event;
	scroll_event _scroll_event;
	key_press_event _key_press_event;

	// user data pointer
	void* _data = nullptr;

	// flags for the supported events
	bool _flag_viewport_event = false;
	bool _flag_pointer_move_event = false;
	bool _flag_key_press_event = false;
	bool _flag_key_release_event = false;
	bool _flag_pointer_press_event = false;
	bool _flag_scroll_event = false;
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
	void set_pointer_move_event(pointer_move_event mme);
	void set_scroll_event(scroll_event mme);
	void set_key_press_event(key_press_event mme);

	void viewportEvent(Magnum::Platform::Sdl2Application::ViewportEvent& event);
	void keyPressEvent(Magnum::Platform::Sdl2Application::KeyEvent& event);
	void keyReleaseEvent(Magnum::Platform::Sdl2Application::KeyEvent& event);
	void pointerPressEvent(Magnum::Platform::Sdl2Application::PointerEvent& event);
	void pointerReleaseEvent(Magnum::Platform::Sdl2Application::PointerEvent& event);
	void pointerMoveEvent(Magnum::Platform::Sdl2Application::PointerMoveEvent& event);
	void ScrollEvent(Magnum::Platform::Sdl2Application::ScrollEvent& event);
	void textInputEvent(Magnum::Platform::Sdl2Application::TextInputEvent& event);
};
} // namespace smg
