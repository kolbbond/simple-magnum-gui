// include header
#include "DrawCallback.hh"

namespace smg {

// constructor
DrawCallback::DrawCallback() {}

// destructor
DrawCallback::~DrawCallback() {}

int DrawCallback::draw() {
	// callback with data
	return _callback(_data);
}

void DrawCallback::pointerMoveEvent(Magnum::Platform::Sdl2Application::PointerMoveEvent& event) {
	// return callbacks pointer
	if(_flag_pointer_move_event) {
		_pointer_move_event(_data, event);
	}
}

void DrawCallback::ScrollEvent(Magnum::Platform::Sdl2Application::ScrollEvent& event) {
	// return callbacks pointer
	if(_flag_scroll_event) {
		_scroll_event(_data, event);
	}
}

void DrawCallback::keyPressEvent(Magnum::Platform::Sdl2Application::KeyEvent& event) {
	// return callbacks pointer
	if(_flag_key_press_event) {
		_key_press_event(_data, event);
	}
}

// setters

void DrawCallback::set_callback(draw_callback fn) {
	// set callback function into object
	_callback = fn;
}

void DrawCallback::set_pointer_move_event(pointer_move_event mme) {
	_flag_pointer_move_event = true;
	_pointer_move_event = mme;
}

void DrawCallback::set_scroll_event(scroll_event mme) {
	_flag_scroll_event = true;
	_scroll_event = mme;
}
void DrawCallback::set_key_press_event(key_press_event mme) {
	_flag_key_press_event = true;
	_key_press_event = mme;
}

void DrawCallback::set_data(void* data) {
	_data = data;
}

// getters

void* DrawCallback::get_data() {
	return _data;
}

// static

// factory
ShDrawCallbackPr DrawCallback::create() {
	return std::make_shared<DrawCallback>();
}

} // namespace smg
