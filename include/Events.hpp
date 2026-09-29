// Event callback types (platform-agnostic Application type)
#pragma once

#include <Corrade/configure.h>
#include <Magnum/Magnum.h>

#if defined(CORRADE_TARGET_EMSCRIPTEN)
#    include <Magnum/Platform/EmscriptenApplication.h>
namespace smg {
using Application = Magnum::Platform::EmscriptenApplication;
}
#else
#    include <Magnum/Platform/Sdl2Application.h>
namespace smg {
using Application = Magnum::Platform::Sdl2Application;
}
#endif

namespace smg {

// legacy void*-style handlers, superseded by DrawCallback::on_*
typedef int (*pointer_move_event)(void*, Application::PointerMoveEvent&);
typedef int (*scroll_event)(void*, Application::ScrollEvent&);
typedef int (*key_press_event)(void*, Application::KeyEvent&);

} // namespace smg
