#include "DrawCallback.hh"

#include <utility>

namespace smg {

DrawCallback::DrawCallback(DrawFn fn) : _draw(std::move(fn)) {}

DrawCallback::DrawCallback(draw_callback callback) { bind_legacy(callback, nullptr, nullptr, nullptr); }

DrawCallback::DrawCallback(draw_callback callback, void* data, key_press_event kpe, pointer_move_event pme, scroll_event se) : _data(data) {
    bind_legacy(callback, kpe, pme, se);
}

ShDrawCallbackPr DrawCallback::create() { return std::make_shared<DrawCallback>(); }

ShDrawCallbackPr DrawCallback::create(DrawFn fn) { return std::make_shared<DrawCallback>(std::move(fn)); }

ShDrawCallbackPr DrawCallback::create(draw_callback callback) {
    ShDrawCallbackPr cb = std::make_shared<DrawCallback>();
    cb->bind_legacy(callback, nullptr, nullptr, nullptr);
    return cb;
}

ShDrawCallbackPr DrawCallback::create(draw_callback callback, void* data, key_press_event kpe, pointer_move_event pme, scroll_event se) {
    ShDrawCallbackPr cb = std::make_shared<DrawCallback>();
    cb->_data = data;
    cb->bind_legacy(callback, kpe, pme, se);
    return cb;
}

DrawCallback& DrawCallback::on_draw(DrawFn fn) {
    _draw = std::move(fn);
    return *this;
}

DrawCallback& DrawCallback::on_key_press(KeyPressFn fn) {
    _key_press = std::move(fn);
    return *this;
}

DrawCallback& DrawCallback::on_pointer_move(PointerMoveFn fn) {
    _pointer_move = std::move(fn);
    return *this;
}

DrawCallback& DrawCallback::on_scroll(ScrollFn fn) {
    _scroll = std::move(fn);
    return *this;
}

int DrawCallback::draw() { return _draw ? _draw() : 0; }

void DrawCallback::keyPressEvent(Application::KeyEvent& event) {
    if(_key_press) _key_press(event);
}

void DrawCallback::pointerMoveEvent(Application::PointerMoveEvent& event) {
    if(_pointer_move) _pointer_move(event);
}

void DrawCallback::ScrollEvent(Application::ScrollEvent& event) {
    if(_scroll) _scroll(event);
}

// legacy handlers read _data at call time, so set_data() may follow set_callback()
void DrawCallback::bind_legacy(draw_callback callback, key_press_event kpe, pointer_move_event pme, scroll_event se) {
    if(callback != nullptr) _draw = [this, callback]() { return callback(_data); };
    if(kpe != nullptr) _key_press = [this, kpe](Application::KeyEvent& e) { return kpe(_data, e); };
    if(pme != nullptr) _pointer_move = [this, pme](Application::PointerMoveEvent& e) { return pme(_data, e); };
    if(se != nullptr) _scroll = [this, se](Application::ScrollEvent& e) { return se(_data, e); };
}

void* DrawCallback::get_data() const { return _data; }

void DrawCallback::set_data(void* data) { _data = data; }

void DrawCallback::set_callback(draw_callback fn) {
    _draw = nullptr;
    bind_legacy(fn, nullptr, nullptr, nullptr);
}

void DrawCallback::set_pointer_move_event(pointer_move_event fn) {
    _pointer_move = nullptr;
    bind_legacy(nullptr, nullptr, fn, nullptr);
}

void DrawCallback::set_scroll_event(scroll_event fn) {
    _scroll = nullptr;
    bind_legacy(nullptr, nullptr, nullptr, fn);
}

void DrawCallback::set_key_press_event(key_press_event fn) {
    _key_press = nullptr;
    bind_legacy(nullptr, fn, nullptr, nullptr);
}

} // namespace smg
