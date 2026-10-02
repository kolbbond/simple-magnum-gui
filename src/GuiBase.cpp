#include <Corrade/configure.h> // For CORRADE_TARGET_EMSCRIPTEN
#include <Corrade/Utility/Arguments.h>
#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <utility>
#include <Magnum/Trade/Trade.h>
#include <imgui.h>
#include <Magnum/Math/Time.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/AbstractImporter.h>

#include "GuiBase.hh"
#include "WindowPlatform.hh"

#ifdef SMG_WITH_IMPLOT3D
#    include "implot3d.h"
#endif

// Implementation-local convenience; the public header no longer leaks these.
using namespace Magnum;
using namespace Magnum::Math::Literals;

namespace smg {

GuiBase::GuiBase(const Arguments& arguments, const GuiConfig& config)
    : Platform::Application{ arguments, NoCreate }, _max_frames(config.max_frames) {

    // prefixed, so Magnum's own --magnum-* options and positional args pass through untouched
    Utility::Arguments args{ "smg" };
    args.addOption("frames", "0").setHelp("frames", "exit after N frames (0 = run until closed)", "N");
    args.parse(arguments.argc, arguments.argv);
    const long frames_flag = args.value<long>("frames");
    if(frames_flag > 0) _max_frames = frames_flag;

    Configuration conf;
    Configuration::WindowFlags flags = Configuration::WindowFlag::Resizable;
    if(config.borderless) flags |= Configuration::WindowFlag::Borderless;
    if(config.always_on_top) flags |= Configuration::WindowFlag::AlwaysOnTop;
    conf.setWindowFlags(flags);
    conf.setSize(config.size);
    conf.setTitle(config.title);
    GLConfiguration glConf;
    if(config.transparent) glConf.setColorBufferSize({ 8, 8, 8, 8 });

#if defined(CORRADE_TARGET_EMSCRIPTEN)
    // WebGL: disable MSAA - not reliably supported
    glConf.setSampleCount(0);

    // create window here (no MSAA fallback needed for WebGL)
    if(!tryCreate(conf, glConf)) { throw std::runtime_error("smg::GuiBase: failed to create WebGL context"); }
#else
    glConf.setSampleCount(config.samples);
    if(!tryCreate(conf, glConf)) {
        Warning() << "smg: no window with" << config.samples << "x MSAA, retrying without";
        if(!tryCreate(conf, glConf.setSampleCount(0))) {
            throw std::runtime_error("smg::GuiBase: failed to create window, with or without MSAA");
        }
    }
#endif

    _lg = Log::create();

#if !defined(CORRADE_TARGET_EMSCRIPTEN)
    _window = Platform::Sdl2Application::window();
    SDL_SetWindowPosition(_window, 0, 0);
    if(config.transparent) {
        _transparent = detail::enable_transparency(_window);
        if(!_transparent) Warning() << "smg: transparent windows are not supported here; the window stays opaque";
    }

    // load window icon (desktop only - no window icon in browser); non-fatal —
    // a missing or broken icon must not abort construction of the whole app
    Magnum::Containers::ArrayView<const char> rawData = Magnum::Utility::Resource{ "image" }.getRaw("smg.jpg");
    PluginManager::Manager<Trade::AbstractImporter> manager;
    Containers::Pointer<Trade::AbstractImporter> importer = manager.loadAndInstantiate("AnyImageImporter");
    if(rawData.isEmpty()) {
        Warning() << "smg: window icon resource 'smg.jpg' missing; skipping icon";
    } else if(!importer || !importer->openData(rawData)) {
        Warning() << "smg: AnyImageImporter could not open the window icon; skipping icon";
    } else {
        _icon = importer->image2D(0); // decode once
        if(_icon) {
            Magnum::ImageView2D icon_view{ _icon->format(), _icon->size(), _icon->data() };
            Platform::Sdl2Application::setWindowIcon(icon_view);
        } else {
            Warning() << "smg: decoding the window icon failed; skipping icon";
        }
    }
#endif

    ImGui::CreateContext();

    const Vector2 size = Vector2{ windowSize() } / dpiScaling();

    // resources are static, so FontDataOwnedByAtlas=false below stops ImGui freeing them
    Containers::ArrayView<const char> font;
    double num_pixels = 18.0f;
    std::vector<std::string> font_names = { "Roboto-Medium.ttf",
        "SourceSansPro-Regular.ttf",
        "DroidSans.ttf",
        "Cousine-Regular.ttf",
        "Karla-Regular.ttf",
        "JetBrainsMonoNerdFont-Regular.ttf" };

    ImGuiIO& io = ImGui::GetIO();

    // Enable docking
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    for(size_t i = 0; i < font_names.size(); i++) {
        ImFontConfig font_cfg;
        font_cfg.FontDataOwnedByAtlas = false;
        font = Utility::Resource{ "font" }.getRaw(font_names[i].c_str());
        snprintf(font_cfg.Name, IM_ARRAYSIZE(font_cfg.Name), "%s, %0.1f px", font_names[i].c_str(), num_pixels);
        ImFont* myfont = io.Fonts->AddFontFromMemoryTTF(const_cast<char*>(font.data()),
            static_cast<int>(font.size()),
            num_pixels * framebufferSize().x() / static_cast<int>(size.x()),
            &font_cfg);
        _fonts.push_back(myfont);
        _fontData.push_back(font);
    }

    // loaded fonts
    _font_default = _fonts[5];
    io.FontDefault = _font_default;

    ImGuiStyle& style = ImGui::GetStyle();
    style.FrameRounding = 8.0f;
    style.WindowRounding = 8.0f;

    // set custom colorscheme
    // transparent
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.2f, 0.2f, 0.2f, 0.2f);

    // blue buttons
    style.Colors[ImGuiCol_Button] = ImVec4(0.2f, 0.4f, 0.7f, 1.0f);

    // lighter blue on hover
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.3f, 0.5f, 1.0f, 1.0f);

    // wrap the existing ImGui context in Magnum's integration
    _imgui =
        ImGuiIntegration::Context(*ImGui::GetCurrentContext(), Vector2{ windowSize() } / dpiScaling(), windowSize(), framebufferSize());

    ImPlot::CreateContext();
#ifdef SMG_WITH_IMPLOT3D
    ImPlot3D::CreateContext();
#endif

#if !defined(MAGNUM_TARGET_WEBGL) && !defined(CORRADE_TARGET_ANDROID)
    // cap the loop at ~16 ms/frame
    Nanoseconds nspf = 16.0_msec;
    setMinimalLoopPeriod(nspf);
#endif
}

void GuiBase::drawBegin() {
    // setup the drawing state
    // clear buffer

    // a panel may have changed the clear color; the compositor shows whatever alpha is left here
    if(_transparent) GL::Renderer::setClearColor(Color4{ 0.0f, 0.0f, 0.0f, 0.0f });
    GL::defaultFramebuffer.clear(GL::FramebufferClear::Color | GL::FramebufferClear::Depth);

    // start a new frame
    _imgui.newFrame();

    ImGuiIO& io = ImGui::GetIO();

    // toggle text input to match what ImGui wants this frame
    if(io.WantTextInput && !isTextInputActive())
        startTextInput();
    else if(!io.WantTextInput && isTextInputActive())
        stopTextInput();

    /* Update application cursor */
    _imgui.updateApplicationCursor(*this);
}

void GuiBase::drawEnd() {
    // draw, reset, swap

    /* Set appropriate states. If you only draw ImGui, it is sufficient to
just enable blending and scissor test in the constructor. */
    GL::Renderer::enable(GL::Renderer::Feature::Blending);
    GL::Renderer::enable(GL::Renderer::Feature::ScissorTest);
    GL::Renderer::disable(GL::Renderer::Feature::FaceCulling);
    GL::Renderer::disable(GL::Renderer::Feature::DepthTest);

    // Set up proper blending to be used by ImGui. There's a great chance
    // you'll need this exact behavior for the rest of your scene. If not, set
    // this only for the drawFrame() call.
    GL::Renderer::setBlendEquation(GL::Renderer::BlendEquation::Add, GL::Renderer::BlendEquation::Add);
    if(_transparent) {
        // alpha accumulates coverage, so over the cleared (0,0,0,0) the result is premultiplied, as DWM expects
        GL::Renderer::setBlendFunction(GL::Renderer::BlendFunction::SourceAlpha, GL::Renderer::BlendFunction::OneMinusSourceAlpha,
            GL::Renderer::BlendFunction::One, GL::Renderer::BlendFunction::OneMinusSourceAlpha);
    } else {
        GL::Renderer::setBlendFunction(GL::Renderer::BlendFunction::SourceAlpha, GL::Renderer::BlendFunction::OneMinusSourceAlpha);
    }

    // draw the frame to background buffer
    _imgui.drawFrame();

    /* Reset state. Only needed if you want to draw something else with
different state after. */
    GL::Renderer::disable(GL::Renderer::Feature::Blending);
    GL::Renderer::disable(GL::Renderer::Feature::ScissorTest);
    GL::Renderer::enable(GL::Renderer::Feature::FaceCulling);
    GL::Renderer::enable(GL::Renderer::Feature::DepthTest);

    // swap background buffers and redraw to screen
    swapBuffers();
    redraw();
}

void GuiBase::drawEvent() {
    // main loop
    // this is called each frame

    // per-frame delta time (clamp spikes from stalls/breakpoints)
    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    if(_have_last_frame) {
        const float elapsed = std::chrono::duration<float>(now - _last_frame).count();
        _dt = elapsed > 0.1f ? 0.1f : elapsed;
    } else {
        _dt = 0.0f;
        _have_last_frame = true;
    }
    _last_frame = now;

    drawBegin();
    draw_callbacks();
    drawEnd();

    if(_max_frames > 0 && ++_frames >= _max_frames) exit();
}

std::pair<int, int> GuiBase::get_window_position() const {
    std::pair<int, int> pos{ 0, 0 };
#if !defined(CORRADE_TARGET_EMSCRIPTEN)
    SDL_GetWindowPosition(_window, &pos.first, &pos.second);
#endif
    return pos;
}

void GuiBase::add_callback(ShDrawCallbackPr callback) {
    if(callback) _callback_list.push_back(std::move(callback));
}

ShDrawCallbackPr GuiBase::add_callback(DrawCallback::DrawFn fn) {
    ShDrawCallbackPr callback = DrawCallback::create(std::move(fn));
    _callback_list.push_back(callback);
    return callback;
}

bool GuiBase::remove_callback(const ShDrawCallbackPr& callback) {
    const auto it = std::find(_callback_list.begin(), _callback_list.end(), callback);
    if(it == _callback_list.end()) return false;
    _callback_list.erase(it);
    return true;
}

void GuiBase::clear_callbacks() { _callback_list.clear(); }

// dispatch over a copy: a handler may add/remove callbacks, and the copy keeps
// every callback alive until this dispatch finishes
void GuiBase::draw_callbacks() {
    const std::vector<ShDrawCallbackPr> callbacks = _callback_list;
    for(const ShDrawCallbackPr& callback : callbacks) {
        if(callback->draw() != 0) _lg->msg("callback error!\n");
    }
}

// setters (desktop only)
#if !defined(CORRADE_TARGET_EMSCRIPTEN)
void GuiBase::set_window_icon(const std::string& icon_file) {
    // a bad runtime path must not kill the app (this is a public setter, unlike
    // the compiled-in startup icon) — log and keep the current icon
    PluginManager::Manager<Trade::AbstractImporter> manager;
    Containers::Pointer<Trade::AbstractImporter> importer = manager.loadAndInstantiate("AnyImageImporter");
    if(!importer || !importer->openFile(icon_file)) {
        Warning() << "smg: could not open icon file" << icon_file.c_str() << "- ignoring";
        return;
    }
    _icon = importer->image2D(0);
    if(!_icon) {
        Warning() << "smg: decoding icon file failed" << icon_file.c_str() << "- ignoring";
        return;
    }
    Magnum::ImageView2D icon_view{ _icon->format(), _icon->size(), _icon->data() };
    Platform::Sdl2Application::setWindowIcon(icon_view);
}

SDL_Window* GuiBase::get_window() const { return _window; }

void GuiBase::set_window_position(int x, int y) { SDL_SetWindowPosition(_window, x, y); }

void GuiBase::set_window_size(int w, int h) { SDL_SetWindowSize(_window, w, h); }

bool GuiBase::set_click_through(bool on) {
    if(!detail::set_click_through(_window, on)) return false;
    _click_through = on;
    return true;
}
#endif

// event handling for imgui
void GuiBase::keyPressEvent(KeyEvent& event) {
    if(_imgui.handleKeyPressEvent(event)) return;

    const std::vector<ShDrawCallbackPr> callbacks = _callback_list;
    for(const ShDrawCallbackPr& callback : callbacks) callback->keyPressEvent(event);
}

void GuiBase::keyReleaseEvent(KeyEvent& event) {
    if(_imgui.handleKeyReleaseEvent(event)) return;
}

void GuiBase::pointerPressEvent(PointerEvent& event) {
    if(_imgui.handlePointerPressEvent(event)) return;
}

void GuiBase::pointerReleaseEvent(PointerEvent& event) {
    if(_imgui.handlePointerReleaseEvent(event)) return;
}

void GuiBase::pointerMoveEvent(PointerMoveEvent& event) {
    // let imgui handle its own events
    if(_imgui.handlePointerMoveEvent(event)) return;

    const std::vector<ShDrawCallbackPr> callbacks = _callback_list;
    for(const ShDrawCallbackPr& callback : callbacks) callback->pointerMoveEvent(event);
}

void GuiBase::scrollEvent(ScrollEvent& event) {
    if(_imgui.handleScrollEvent(event)) {
        /* Prevent scrolling the page */
        event.setAccepted();
        return;
    }

    const std::vector<ShDrawCallbackPr> callbacks = _callback_list;
    for(const ShDrawCallbackPr& callback : callbacks) callback->ScrollEvent(event);
}

void GuiBase::textInputEvent(TextInputEvent& event) {
    if(_imgui.handleTextInputEvent(event)) return;
}

void GuiBase::viewportEvent(ViewportEvent& event) {
    GL::defaultFramebuffer.setViewport({ {}, event.framebufferSize() });

    _imgui.relayout(Vector2{ event.windowSize() } / event.dpiScaling(), event.windowSize(), event.framebufferSize());
}
} // namespace smg
