#pragma once

#include <memory>
#include <Magnum/Magnum.h>
#include <Magnum/ImGuiIntegration/Integration.h>
#include <Magnum/Platform/Sdl2Application.h>

#include "Events.hpp"

namespace smg {

typedef std::shared_ptr<class DrawCallback> ShDrawCallbackPr;

// for users to implement callbacks
// @hey: what else does user need to integrate?
typedef int (*draw_callback)(void*);

using namespace Magnum;

class DrawCallback {
protected:
	// draw callback function
	draw_callback _callback; // callback function

    // @hey: refactor into one event holder?
    pointer_move_event _pointer_move_event;
	scroll_event _scroll_event;
	key_press_event _key_press_event;
	void* _data = nullptr; // user data pointer

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
	DrawCallback(draw_callback callback);
	DrawCallback(draw_callback callback, void* data, key_press_event kpe, pointer_move_event pme, scroll_event se);

	// destructor
	~DrawCallback();

	// call the callback
	int draw();

	// factory
	static ShDrawCallbackPr create();
	static ShDrawCallbackPr create(draw_callback callback);
	static ShDrawCallbackPr create(draw_callback callback, void* data, key_press_event kpe, pointer_move_event pme, scroll_event se);

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
