// include header
#include "DrawCallback.hh"

namespace smg {

// constructor
DrawCallback::DrawCallback() {}
DrawCallback::DrawCallback(draw_callback callback) {
	set_callback(callback);
}

// overloaded constructor with all necessary inputs
DrawCallback::DrawCallback(draw_callback callback, void* data, key_press_event kpe, pointer_move_event pme, scroll_event se) {
	set_callback(callback);
	set_data(data);
	set_key_press_event(kpe);
	set_pointer_move_event(pme);
	set_scroll_event(se);
}

// factory
ShDrawCallbackPr DrawCallback::create() {
	return std::make_shared<DrawCallback>();
}

ShDrawCallbackPr DrawCallback::create(draw_callback callback) {
	return std::make_shared<DrawCallback>(callback);
}

ShDrawCallbackPr DrawCallback::create(draw_callback callback, void* data, key_press_event kpe, pointer_move_event pme, scroll_event se) {
	return std::make_shared<DrawCallback>(callback, data, kpe, pme, se);
}
// destructor
DrawCallback::~DrawCallback() {
	//std::printf("[X] DrawCallback destructor\n");
}

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


} // namespace smg
