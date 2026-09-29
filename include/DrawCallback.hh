#pragma once

#include <functional>
#include <memory>

#include <Magnum/ImGuiIntegration/Integration.h> // kept: consumers rely on it transitively

#include "Events.hpp"

namespace smg {

typedef std::shared_ptr<class DrawCallback> ShDrawCallbackPr;

// legacy draw handler, superseded by DrawCallback::on_draw
typedef int (*draw_callback)(void*);

// A draw function plus optional event handlers, registered with GuiBase::add_callback.
// Handlers return 0 on success; an empty handler (or nullptr) is simply not called.
class DrawCallback {
public:
    using DrawFn = std::function<int()>;
    using KeyPressFn = std::function<int(Application::KeyEvent&)>;
    using PointerMoveFn = std::function<int(Application::PointerMoveEvent&)>;
    using ScrollFn = std::function<int(Application::ScrollEvent&)>;

    DrawCallback() = default;
    explicit DrawCallback(DrawFn fn);
    [[deprecated("use DrawCallback(DrawFn) with a capturing lambda")]] explicit DrawCallback(draw_callback callback);
    [[deprecated("use DrawCallback(DrawFn) + on_* with capturing lambdas")]] DrawCallback(draw_callback callback,
        void* data,
        key_press_event kpe,
        pointer_move_event pme,
        scroll_event se);

    // legacy wrappers bind `this` to read _data at call time -> identity type
    DrawCallback(const DrawCallback&) = delete;
    DrawCallback& operator=(const DrawCallback&) = delete;

    static ShDrawCallbackPr create();
    static ShDrawCallbackPr create(DrawFn fn);
    [[deprecated("use create(DrawFn) with a capturing lambda")]] static ShDrawCallbackPr create(draw_callback callback);
    [[deprecated("use create(DrawFn) + on_* with capturing lambdas")]] static ShDrawCallbackPr
    create(draw_callback callback, void* data, key_press_event kpe, pointer_move_event pme, scroll_event se);

    DrawCallback& on_draw(DrawFn fn);
    DrawCallback& on_key_press(KeyPressFn fn);
    DrawCallback& on_pointer_move(PointerMoveFn fn);
    DrawCallback& on_scroll(ScrollFn fn);

    // dispatch, called by GuiBase
    int draw();
    void keyPressEvent(Application::KeyEvent& event);
    void pointerMoveEvent(Application::PointerMoveEvent& event);
    void ScrollEvent(Application::ScrollEvent& event);

    [[deprecated("capture state in the on_* lambdas instead")]] [[nodiscard]] void* get_data() const;
    [[deprecated("capture state in the on_* lambdas instead")]] void set_data(void* data);
    [[deprecated("use on_draw")]] void set_callback(draw_callback fn);
    [[deprecated("use on_pointer_move")]] void set_pointer_move_event(pointer_move_event fn);
    [[deprecated("use on_scroll")]] void set_scroll_event(scroll_event fn);
    [[deprecated("use on_key_press")]] void set_key_press_event(key_press_event fn);

private:
    void bind_legacy(draw_callback callback, key_press_event kpe, pointer_move_event pme, scroll_event se);

    DrawFn _draw;
    KeyPressFn _key_press;
    PointerMoveFn _pointer_move;
    ScrollFn _scroll;
    void* _data = nullptr; // legacy only
};

} // namespace smg
