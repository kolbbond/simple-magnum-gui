#pragma once

#include <memory>

namespace guild {

typedef std::shared_ptr<class DrawCallback> ShDrawCallbackPr;

// for users to implement callbacks
// @hey, what else does user need to integrate?
typedef int (*draw_callback)(void*);

class DrawCallback {
protected:
	// draw callback function
	draw_callback callback_;

	// user data pointer
	void* data_ = nullptr;

public:
	// constructor
	DrawCallback();

	// destructor
	~DrawCallback();

	// call the callback
	int call();

	// factory
	static ShDrawCallbackPr create();

	void* get_data();
	void set_callback(draw_callback);
	void set_data(void*);
};
} // namespace guild
