#pragma once

#include <memory>
#include <Corrade/configure.h>
#include <Magnum/Magnum.h>
#include <Magnum/ImGuiIntegration/Integration.h>

// Platform-specific application
#if defined(CORRADE_TARGET_EMSCRIPTEN)
#include <Magnum/Platform/EmscriptenApplication.h>
namespace smg { using Application = Magnum::Platform::EmscriptenApplication; }
#else
#include <Magnum/Platform/Sdl2Application.h>
namespace smg { using Application = Magnum::Platform::Sdl2Application; }
#endif

#include "Events.hpp"

namespace smg {

typedef std::shared_ptr<class DrawCallback> ShDrawCallbackPr;

// for users to implement callbacks
typedef int (*draw_callback)(void*);

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

	[[nodiscard]] void* get_data() const;
	void set_callback(draw_callback);
	void set_data(void*);
	void set_pointer_move_event(pointer_move_event mme);
	void set_scroll_event(scroll_event mme);
	void set_key_press_event(key_press_event mme);

	void viewportEvent(Application::ViewportEvent& event);
	void keyPressEvent(Application::KeyEvent& event);
	void keyReleaseEvent(Application::KeyEvent& event);
	void pointerPressEvent(Application::PointerEvent& event);
	void pointerReleaseEvent(Application::PointerEvent& event);
	void pointerMoveEvent(Application::PointerMoveEvent& event);
	void ScrollEvent(Application::ScrollEvent& event);
	void textInputEvent(Application::TextInputEvent& event);
};
} // namespace smg
