// default Events
#pragma once
#include "DrawCallback.hh"
#include <iostream>
#include <ostream>

namespace smg {

// event definitions
typedef int (*pointer_move_event)(void*, Magnum::Platform::Sdl2Application::PointerMoveEvent&);
typedef int (*scroll_event)(void*, Magnum::Platform::Sdl2Application::ScrollEvent&);
typedef int (*key_press_event)(void*, Magnum::Platform::Sdl2Application::KeyEvent&);

class Events {
public:
	// Event storage
	pointer_move_event _pme;
	key_press_event _kpe;
	scroll_event _se;

	// static defaults
	static int pointer_move_event_default(void* data, Magnum::Platform::Sdl2Application::PointerMoveEvent& event) {
        (void) data;

		using namespace Magnum::Math::Literals;

		// if no pointer button, return
		if(!(event.pointers() & Magnum::Platform::Sdl2Application::Pointer::MouseLeft) &&
			(!(event.pointers() & Magnum::Platform::Sdl2Application::Pointer::MouseRight)) &&
			(!(event.pointers() & Magnum::Platform::Sdl2Application::Pointer::MouseMiddle)) &&
			(!(event.pointers() & Magnum::Platform::Sdl2Application::Pointer::MouseButton4)) &&
			(!(event.pointers() & Magnum::Platform::Sdl2Application::Pointer::MouseButton5)))
			return 0;

		// @hey, we need to get SDL window size from here
		//Vector2 delta = 3.0f * Vector2{event.relativePosition()} / Vector2{ImGui::GetWindowSize()};
		//Magnum::Vector2 delta = 3.0f * Magnum::Vector2{ event.relativePosition() } / Magnum::Vector2{ 1920, 1080 };

		// if left click
		// we rotate
		if((event.pointers() & Magnum::Platform::Sdl2Application::Pointer::MouseLeft)) {
			event.setAccepted();
		} else if((event.pointers() & Magnum::Platform::Sdl2Application::Pointer::MouseRight)) {
		} else if((event.pointers() & Magnum::Platform::Sdl2Application::Pointer::MouseMiddle)) {
			event.setAccepted();

		} else if((event.pointers() & Magnum::Platform::Sdl2Application::Pointer::MouseButton4)) {
		}

		//    redraw();

		return 0;
	}

	static int scroll_event_default(void* data, Magnum::Platform::Sdl2Application::ScrollEvent& event) {
		(void)data;
        (void)event;

		return 0;
	}

	static int key_press_event_default(void* data, Magnum::Platform::Sdl2Application::KeyEvent& event) {
		(void)data;

		std::cout << "key: " << event.keyName().data() << std::endl;

		// keyboard input checking
		if(event.key() == Magnum::Platform::Sdl2Application::Key::Up) {
		} else if(event.key() == Magnum::Platform::Sdl2Application::Key::Down) {
		} else if(event.key() == Magnum::Platform::Sdl2Application::Key::Right) {
		} else if(event.key() == Magnum::Platform::Sdl2Application::Key::Left) {
		}

		else if(event.key() == Magnum::Platform::Sdl2Application::Key::D) {
		} else if(event.key() == Magnum::Platform::Sdl2Application::Key::P) {
		} else if(event.key() == Magnum::Platform::Sdl2Application::Key::S) {
		}

		return 0;
	}
};
} // namespace smg
