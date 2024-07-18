// DrawCallback.cpp
// ###### -

// include header
#include "DrawCallback.hh"

namespace guild {

// constructor
DrawCallback::DrawCallback() {}

// destructor
DrawCallback::~DrawCallback() {}

int DrawCallback::draw() {
	// callback with data
	return _callback(_data);
}

void DrawCallback::mouseMoveEvent(Magnum::Platform::Sdl2Application::MouseMoveEvent& event) {
	// return callbacks mouse
	if(_flag_mouse_move_event) {
		_mouse_move_event(_data, event);
	}
}

void DrawCallback::mouseScrollEvent(Magnum::Platform::Sdl2Application::MouseScrollEvent& event) {
	// return callbacks mouse
	if(_flag_mouse_scroll_event) {
		_mouse_scroll_event(_data, event);
	}
}

void DrawCallback::keyPressEvent(Magnum::Platform::Sdl2Application::KeyEvent& event) {
	// return callbacks mouse
	if(_flag_key_press_event) {
		_key_press_event(_data, event);
	}
}

// setters

void DrawCallback::set_callback(draw_callback fn) {
	// set callback function into object
	_callback = fn;
}

void DrawCallback::set_mouse_move_event(mouse_move_event mme) {
	_flag_mouse_move_event = true;
	_mouse_move_event = mme;
}

void DrawCallback::set_mouse_scroll_event(mouse_scroll_event mme) {
	_flag_mouse_scroll_event = true;
	_mouse_scroll_event = mme;
}
void DrawCallback::set_key_press_event(key_press_event mme) {
	_flag_key_press_event = true;
	_key_press_event = mme;
}

/*
void DrawCallback::add_callback(draw_callback fn) {
	// add callback function into object
	callback_ = fn;
}
*/

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

} // namespace guild
