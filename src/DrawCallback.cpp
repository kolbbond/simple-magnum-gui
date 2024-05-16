// DrawCallback.cpp
// ###### -

// include header
#include "DrawCallback.hh"

namespace guild {

// constructor
DrawCallback::DrawCallback() {}

// destructor
DrawCallback::~DrawCallback() {}

int DrawCallback::call() {
	// callback with data
	return callback_(data_);
}

// setters

void DrawCallback::set_callback(draw_callback fn) {
	// set callback function into object
	callback_ = fn;
}

void DrawCallback::set_data(void* data) {
	data_ = data;
}

// getters

void* DrawCallback::get_data() {
	return data_;
}

// static

// factory
ShDrawCallbackPr DrawCallback::create() {
	return std::make_shared<DrawCallback>();
}

} // namespace guild
