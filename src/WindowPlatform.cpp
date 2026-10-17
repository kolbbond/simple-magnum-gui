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
#else
#    include <cstdlib>
#    include <cstring>
#    include <string>
#    include <sys/socket.h>
#    include <sys/un.h>
#    include <unistd.h>
#    if defined(SMG_HAVE_WAYLAND)
#        include <wayland-client.h>
#    endif
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
    // a freshly layered window shows nothing until it has attributes; full opacity leaves the
    // framebuffer alpha (blur-behind) in charge
    if(on) {
        if(layered)
            SetLayeredWindowAttributes(hwnd, key, alpha, flags);
        else
            SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
    }
    return true;
}

unsigned int transparent_window_flags() { return 0; }
bool compositor_float(SDL_Window*, int, int, bool, bool) { return false; }
bool compositor_move(SDL_Window*, int, int) { return false; }

#else

namespace {

// SDL3's SDL_WINDOW_TRANSPARENT; without it SDL3 presents EGL surfaces opaque (EGL_EXT_present_opaque)
constexpr unsigned int Sdl3WindowTransparent = 0x40000000u;

bool sdl2_compat() {
    // sdl2-compat reports patch levels from 50 up
    SDL_version v;
    SDL_GetVersion(&v);
    return v.patch >= 50;
}

bool on_wayland(SDL_Window* window, SDL_SysWMinfo& info) {
    SDL_VERSION(&info.version);
    return window && SDL_GetWindowWMInfo(window, &info) && info.subsystem == SDL_SYSWM_WAYLAND;
}

// one IPC request, "[[BATCH]]a;b" for several; the window is picked by our pid
bool hyprland_request(const std::string& request) {
    const char* runtime = std::getenv("XDG_RUNTIME_DIR");
    const char* instance = std::getenv("HYPRLAND_INSTANCE_SIGNATURE");
    if(!runtime || !instance) return false;
    const std::string path = std::string(runtime) + "/hypr/" + instance + "/.socket.sock";
    sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    if(path.size() >= sizeof(addr.sun_path)) return false;
    std::memcpy(addr.sun_path, path.c_str(), path.size() + 1);

    const int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if(fd < 0) return false;
    bool ok = connect(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == 0 &&
        write(fd, request.data(), request.size()) == static_cast<ssize_t>(request.size());
    char reply[512];
    const ssize_t n = ok ? read(fd, reply, sizeof(reply) - 1) : -1;
    close(fd);
    if(n <= 0) return false;
    reply[n] = '\0';
    return std::strncmp(reply, "ok", 2) == 0;
}

std::string pid_selector() { return "pid:" + std::to_string(getpid()); }

#    if defined(SMG_HAVE_WAYLAND)
void registry_global(void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t) {
    if(std::strcmp(interface, wl_compositor_interface.name) == 0)
        *static_cast<wl_compositor**>(data) = static_cast<wl_compositor*>(wl_registry_bind(registry, name, &wl_compositor_interface, 1));
}
void registry_global_remove(void*, wl_registry*, uint32_t) {}

// SDL keeps its wl_compositor to itself; bind our own on a private queue so SDL's events are untouched
wl_compositor* compositor(wl_display* display) {
    static wl_compositor* bound = nullptr;
    if(bound) return bound;
    wl_event_queue* queue = wl_display_create_queue(display);
    wl_display* wrapper = static_cast<wl_display*>(wl_proxy_create_wrapper(display));
    wl_proxy_set_queue(reinterpret_cast<wl_proxy*>(wrapper), queue);
    wl_registry* registry = wl_display_get_registry(wrapper);
    wl_proxy_wrapper_destroy(wrapper);
    static const wl_registry_listener listener = { registry_global, registry_global_remove };
    wl_registry_add_listener(registry, &listener, static_cast<void*>(&bound));
    wl_display_roundtrip_queue(display, queue);
    wl_registry_destroy(registry);
    if(bound) wl_proxy_set_queue(reinterpret_cast<wl_proxy*>(bound), nullptr);
    wl_event_queue_destroy(queue);
    return bound;
}
#    endif

} // namespace

unsigned int transparent_window_flags() { return sdl2_compat() ? Sdl3WindowTransparent : 0; }

bool enable_transparency(SDL_Window* window) {
    // the compositor blends by alpha once SDL3 got the flag at creation; X11 with real SDL2 is not wired up
    return window && sdl2_compat() && (SDL_GetWindowFlags(window) & Sdl3WindowTransparent);
}

bool set_click_through(SDL_Window* window, bool on) {
#    if defined(SMG_HAVE_WAYLAND)
    SDL_SysWMinfo info;
    if(!on_wayland(window, info)) return false;
    // empty input region: clicks fall through; null restores the whole surface. Applied on the next swap
    if(!on) {
        wl_surface_set_input_region(info.info.wl.surface, nullptr);
        return true;
    }
    wl_compositor* c = compositor(info.info.wl.display);
    if(!c) return false;
    wl_region* empty = wl_compositor_create_region(c);
    wl_surface_set_input_region(info.info.wl.surface, empty);
    wl_region_destroy(empty);
    return true;
#    else
    // X11 (an XFixes input shape) is not wired up yet
    (void)window;
    (void)on;
    return false;
#    endif
}

bool compositor_float(SDL_Window* window, int w, int h, bool on_top, bool borderless) {
    SDL_SysWMinfo info;
    if(!on_wayland(window, info)) return false;
    const std::string sel = pid_selector();
    // a tiled window can't be dragged or placed; pin keeps it over every workspace
    std::string request = "[[BATCH]]dispatch setfloating " + sel;
    request += ";dispatch resizewindowpixel exact " + std::to_string(w) + " " + std::to_string(h) + "," + sel;
    if(on_top) request += ";dispatch pin " + sel;
    if(borderless) request += ";dispatch setprop " + sel + " border_size 0;dispatch setprop " + sel + " no_shadow 1";
    return hyprland_request(request);
}

bool compositor_move(SDL_Window* window, int x, int y) {
    SDL_SysWMinfo info;
    if(!on_wayland(window, info)) return false;
    return hyprland_request("dispatch movewindowpixel exact " + std::to_string(x) + " " + std::to_string(y) + "," + pid_selector());
}

#endif

} // namespace smg::detail
