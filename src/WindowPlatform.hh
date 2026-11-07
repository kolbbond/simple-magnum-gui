// native window tweaks SDL2 has no API for; kept out of GuiBase.cpp so platform headers
// (windows.h macros) never meet Magnum's
#pragma once

struct SDL_Window;

namespace smg::detail {

// let the compositor blend the window by the framebuffer's alpha; needs an alpha color buffer
bool enable_transparency(SDL_Window* window);

// pass mouse input through to whatever is below the window
bool set_click_through(SDL_Window* window, bool on);

// extra SDL_CreateWindow flags a transparent window needs; 0 where none
unsigned int transparent_window_flags();

// Wayland lets only the compositor place windows, so these go through Hyprland's IPC; the window
// must already be mapped. false elsewhere. Floating restores `w` x `h`, the size it had before tiling
bool compositor_float(SDL_Window* window, int w, int h, bool on_top, bool borderless);
bool compositor_move(SDL_Window* window, int x, int y);

} // namespace smg::detail
