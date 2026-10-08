// main gui class
#pragma once

#include <chrono>
#include <string>
#include <tuple>
#include <vector>

// magnum includes
#include <Corrade/configure.h>
#include <Magnum/Magnum.h>

// Platform-specific application
#if defined(CORRADE_TARGET_EMSCRIPTEN)
#    include <Magnum/Platform/EmscriptenApplication.h>
#else
#    include <Magnum/Platform/Sdl2Application.h>
#    include "SDL_video.h"
#endif

#include <Corrade/Containers/Optional.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Framebuffer.h>
#include <Magnum/GL/Renderbuffer.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/ImGuiIntegration/Context.hpp>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Shaders/VertexColorGL.h>
#include <Magnum/Image.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/Trade.h>

#include "log.hh"

#include "imgui.h"

// smg includes
#include "DrawCallback.hh"
#include "implot.h"

namespace smg {

// window/loop setup; namespace scope so it can default-construct as a GuiBase ctor argument
struct GuiConfig {
    std::string title = "smg";
    Magnum::Vector2i size{ 1600, 1000 };
    int samples = 4; // MSAA; falls back to none if the context can't provide it
    long max_frames = 0; // exit after this many frames; 0 = until closed. `--smg-frames N` overrides

    // desktop overlays. transparent: the window is blended by the framebuffer alpha, which drawBegin
    // clears to 0, so only what you draw shows (Windows; elsewhere the window stays opaque)
    bool transparent = false;
    bool borderless = false;
    bool always_on_top = false;

    // no visible window: draw into an off-screen framebuffer of `size` (tests, CI, batch
    // screenshots). `--smg-hidden` sets it
    bool hidden = false;
};

// base gui class, entry point for guis

class GuiBase: public Magnum::Platform::Application {

protected:
    // our imgui context
    Magnum::ImGuiIntegration::Context _imgui{ Magnum::NoCreate };

#if !defined(CORRADE_TARGET_EMSCRIPTEN)
    // actual window (desktop only, assume SDL); non-owning, owned by Platform::Application
    SDL_Window* _window = nullptr;
#endif

    // logger (safe no-op default until the constructor installs a real Log)
    ShLogPr _lg = NullLog::create();

    // font setting
    std::vector<Corrade::Containers::ArrayView<const char>> _fontData;
    std::vector<ImFont*> _fonts;
    ImFont* _font_default = nullptr;

    // icon settings
    Corrade::Containers::Optional<Magnum::Trade::ImageData2D> _icon;

    // list of set callbacks
    std::vector<ShDrawCallbackPr> _callback_list;

    // per-frame delta time (seconds), updated at the top of drawEvent
    float _dt = 0.0f;
    std::chrono::steady_clock::time_point _last_frame{};
    bool _have_last_frame = false;

    long _max_frames = 0;
    long _frames = 0;

    bool _transparent = false; // window actually composited by alpha
    bool _click_through = false;

    // hidden mode target, plus pending captures (taken at the end of a frame)
    Corrade::Containers::Optional<Magnum::GL::Renderbuffer> _offscreen_color;
    Corrade::Containers::Optional<Magnum::GL::Renderbuffer> _offscreen_depth;
    Corrade::Containers::Optional<Magnum::GL::Framebuffer> _offscreen;
    std::string _shot_path;
    bool _shot_with_ui = true;
    std::string _cli_shot;
    std::string _record_dir;
    int _record_every = 1;
    bool _record_with_ui = true;
    long _record_index = 0;

    void capture_pending(bool ui_drawn);


public:
    // throws std::runtime_error if no window/GL context can be created
    explicit GuiBase(const Arguments& arguments, const GuiConfig& config = GuiConfig{});

    ~GuiBase();

    // draw callbacks
    // main draw event loop (called every iteration)
    void drawEvent() override;
    void drawBegin();
    void drawEnd();
    void draw_callbacks();

    // callbacks run in registration order; add/remove take effect from the next dispatch
    void add_callback(ShDrawCallbackPr callback);
    ShDrawCallbackPr add_callback(DrawCallback::DrawFn fn); // returns the handle for remove_callback
    bool remove_callback(const ShDrawCallbackPr& callback);
    void clear_callbacks();

    // the framebuffer the app draws into: the window's, or the off-screen one when hidden. Bind
    // this, not GL::defaultFramebuffer, after rendering into your own framebuffers
    static Magnum::GL::AbstractFramebuffer& main_framebuffer();
    [[nodiscard]] Magnum::Vector2i framebuffer_size() const;
    [[nodiscard]] bool hidden() const { return bool(_offscreen); }

    // capture: screenshot() is taken at the end of the current frame (with_ui = false leaves out
    // the ImGui layer); recording writes dir/frame_00000.png every `every` frames until stopped.
    // `--smg-screenshot out.png` (on the last of `--smg-frames`, default 60) and `--smg-record dir`
    void screenshot(const std::string& path, bool with_ui = true);
    void start_recording(const std::string& dir, int every = 1, bool with_ui = true);
    void stop_recording() { _record_dir.clear(); }
    [[nodiscard]] bool recording() const { return !_record_dir.empty(); }
    // the main framebuffer now, RGBA8; alpha is forced opaque unless the window is transparent
    Magnum::Image2D grab();

    // seconds since the previous frame (0 on the first frame, spike-clamped)
    [[nodiscard]] float dt() const { return _dt; }

    // getters (some are desktop-only)
    [[nodiscard]] std::pair<int, int> get_window_position() const;
#if !defined(CORRADE_TARGET_EMSCRIPTEN)
    [[nodiscard]] SDL_Window* get_window() const;
    void set_window_icon(const std::string& icon_file);
    void set_window_position(int x, int y);
    void set_window_size(int x, int y);

    // mouse input goes to whatever is below the window; the window then gets no mouse events,
    // so pair it with a way back (a global hotkey, a tray icon). false where unsupported (non-Windows)
    bool set_click_through(bool on);
    [[nodiscard]] bool click_through() const { return _click_through; }
    [[nodiscard]] bool transparent() const { return _transparent; }
#endif

    // event wrappers
    void viewportEvent(ViewportEvent& event) override;
    void keyPressEvent(KeyEvent& event) override;
    void keyReleaseEvent(KeyEvent& event) override;
    void pointerPressEvent(PointerEvent& event) override;
    void pointerReleaseEvent(PointerEvent& event) override;
    void pointerMoveEvent(PointerMoveEvent& event) override;
    void scrollEvent(ScrollEvent& event) override;
    void textInputEvent(TextInputEvent& event) override;
};
} // namespace smg
