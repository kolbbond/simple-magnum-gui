// Event callback typedefs (platform-agnostic Application type)
#pragma once

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

} // namespace smg
