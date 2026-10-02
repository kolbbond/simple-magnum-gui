#include "WindowPlatform.hh"

#include <SDL_syswm.h>

#if defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    include <windows.h>
#    include <dwmapi.h>
#endif

namespace smg::detail {

#if defined(_WIN32)

namespace {

HWND native_handle(SDL_Window* window) {
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if(!window || !SDL_GetWindowWMInfo(window, &info) || info.subsystem != SDL_SYSWM_WINDOWS) return nullptr;
    return info.info.win.window;
}

} // namespace

bool enable_transparency(SDL_Window* window) {
    const HWND hwnd = native_handle(window);
    if(!hwnd) return false;
    // DWM composites a blur-behind window by its alpha; an empty region means no actual blur
    // (the approach GLFW uses for GLFW_TRANSPARENT_FRAMEBUFFER on Windows 8+)
    const HRGN region = CreateRectRgn(0, 0, -1, -1);
    DWM_BLURBEHIND bb = {};
    bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
    bb.hRgnBlur = region;
    bb.fEnable = TRUE;
    const HRESULT hr = DwmEnableBlurBehindWindow(hwnd, &bb);
    DeleteObject(region);
    return SUCCEEDED(hr);
}

bool set_click_through(SDL_Window* window, bool on) {
    const HWND hwnd = native_handle(window);
    if(!hwnd) return false;
    // WS_EX_TRANSPARENT only skips hit-testing across processes on a layered window
    LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    COLORREF key = 0;
    BYTE alpha = 0;
    DWORD flags = 0;
    const bool layered = (ex & WS_EX_LAYERED) && GetLayeredWindowAttributes(hwnd, &key, &alpha, &flags);
    if(on) {
        ex |= WS_EX_LAYERED | WS_EX_TRANSPARENT;
    } else {
        ex &= ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT);
        if(!(layered && (flags & (LWA_ALPHA | LWA_COLORKEY)))) ex &= ~static_cast<LONG_PTR>(WS_EX_LAYERED);
    }
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, ex);
    // re-apply whatever attributes it had (none: zero flags keep the framebuffer alpha in charge)
    if(on) SetLayeredWindowAttributes(hwnd, key, alpha, flags);
    return true;
}

#else

// X11 (an ARGB visual + XFixes input shape) and Wayland (an empty input region) are not wired up yet
bool enable_transparency(SDL_Window*) { return false; }
bool set_click_through(SDL_Window*, bool) { return false; }

#endif

} // namespace smg::detail
