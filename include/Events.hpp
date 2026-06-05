// default Events
#pragma once

#include <iostream>
#include <ostream>
#include <Corrade/configure.h>
#include <Magnum/Magnum.h>

// Platform-specific application (must match DrawCallback.hh)
#if defined(CORRADE_TARGET_EMSCRIPTEN)
#include <Magnum/Platform/EmscriptenApplication.h>
namespace smg { using Application = Magnum::Platform::EmscriptenApplication; }
#else
#include <Magnum/Platform/Sdl2Application.h>
namespace smg { using Application = Magnum::Platform::Sdl2Application; }
#endif

namespace smg {

// event definitions using platform-agnostic Application type
typedef int (*pointer_move_event)(void*, Application::PointerMoveEvent&);
typedef int (*scroll_event)(void*, Application::ScrollEvent&);
typedef int (*key_press_event)(void*, Application::KeyEvent&);

class Events {
public:
	// Event storage
	pointer_move_event _pme;
	key_press_event _kpe;
	scroll_event _se;

	// static defaults
	static int pointer_move_event_default(void* data, Application::PointerMoveEvent& event) {
        (void) data;

		using namespace Magnum::Math::Literals;

		// if no pointer button, return
		if(!(event.pointers() & Application::Pointer::MouseLeft) &&
			(!(event.pointers() & Application::Pointer::MouseRight)) &&
			(!(event.pointers() & Application::Pointer::MouseMiddle)) &&
			(!(event.pointers() & Application::Pointer::MouseButton4)) &&
			(!(event.pointers() & Application::Pointer::MouseButton5)))
			return 0;

		// if left click we rotate
		if((event.pointers() & Application::Pointer::MouseLeft)) {
			event.setAccepted();
		} else if((event.pointers() & Application::Pointer::MouseRight)) {
		} else if((event.pointers() & Application::Pointer::MouseMiddle)) {
			event.setAccepted();
		} else if((event.pointers() & Application::Pointer::MouseButton4)) {
		}

		return 0;
	}

	static int scroll_event_default(void* data, Application::ScrollEvent& event) {
		(void)data;
        (void)event;

		return 0;
	}

	static int key_press_event_default(void* data, Application::KeyEvent& event) {
		(void)data;

		std::cout << "key: " << event.keyName().data() << std::endl;

		// keyboard input checking
		if(event.key() == Application::Key::Up) {
		} else if(event.key() == Application::Key::Down) {
		} else if(event.key() == Application::Key::Right) {
		} else if(event.key() == Application::Key::Left) {
		}

		else if(event.key() == Application::Key::D) {
		} else if(event.key() == Application::Key::P) {
		} else if(event.key() == Application::Key::S) {
		}

		return 0;
	}
};
} // namespace smg
