// native window tweaks SDL2 has no API for; kept out of GuiBase.cpp so platform headers
// (windows.h macros) never meet Magnum's
#pragma once

struct SDL_Window;

namespace smg::detail {

// let the compositor blend the window by the framebuffer's alpha; needs an alpha color buffer
bool enable_transparency(SDL_Window* window);

// pass mouse input through to whatever is below the window
bool set_click_through(SDL_Window* window, bool on);

} // namespace smg::detail
