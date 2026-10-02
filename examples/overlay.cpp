// desktop overlay: a transparent, borderless, always-on-top HUD (clock, CPU load, FPS).
// Drag the panel to move it. Click-through lets the mouse reach the windows below; while it is on
// the overlay can't be clicked, so Ctrl+Alt+O (a global hotkey, Windows) toggles it back.
//   overlay [--pos X Y] [--click-through]

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include <SDL_mouse.h>
#include <SDL_system.h>
#include <SDL_video.h>

#include "GuiBase.hh"
#include "imgui.h"
#include "implot.h"

#if defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    include <windows.h>
#else
#    include <fstream>
#    include <sstream>
#endif

namespace {

constexpr int HotkeyId = 1;
constexpr int HistoryLength = 120; // samples, one per 0.5 s

#if defined(_WIN32)
std::uint64_t fileTime(const FILETIME& f) { return (static_cast<std::uint64_t>(f.dwHighDateTime) << 32) | f.dwLowDateTime; }
#endif

// whole-machine CPU busy fraction since the previous call
class CpuLoad {
public:
    float sample() {
        std::uint64_t idle = 0;
        std::uint64_t total = 0;
        read(idle, total);
        const std::uint64_t dIdle = idle - _idle;
        const std::uint64_t dTotal = total - _total;
        _idle = idle;
        _total = total;
        return dTotal > 0 ? 100.0f * (1.0f - static_cast<float>(dIdle) / static_cast<float>(dTotal)) : 0.0f;
    }

private:
    std::uint64_t _idle = 0;
    std::uint64_t _total = 0;

    static void read(std::uint64_t& idle, std::uint64_t& total) {
#if defined(_WIN32)
        FILETIME i, k, u;
        if(!GetSystemTimes(&i, &k, &u)) return;
        idle = fileTime(i);
        total = fileTime(k) + fileTime(u); // kernel time includes idle
#else
        std::ifstream stat("/proc/stat");
        std::string cpu;
        std::uint64_t v[8] = {};
        stat >> cpu;
        for(std::uint64_t& x : v) stat >> x;
        idle = v[3] + v[4];
        for(std::uint64_t x : v) total += x;
#endif
    }
};

#if defined(_WIN32)
bool g_hotkeyPressed = false;

// SDL hands every Win32 message to this before dispatching; WM_HOTKEY arrives on the thread queue
void SDLCALL messageHook(void*, void*, unsigned int message, Uint64 wParam, Sint64) {
    if(message == WM_HOTKEY && wParam == HotkeyId) g_hotkeyPressed = true;
}
#endif

// widgets the OS must not treat as drag handle, in ImGui coordinates, refreshed every frame
struct DragState {
    std::vector<ImVec4> widgets; // min.xy, max.xy
    ImVec2 imguiSize{ 1.0f, 1.0f };
};

// the OS drags a borderless window when the hit test says so; works at any DPI scale, unlike
// moving it by hand from mouse deltas
SDL_HitTestResult SDLCALL hitTest(SDL_Window* window, const SDL_Point* p, void* data) {
    const DragState* state = static_cast<const DragState*>(data);
    int w = 0;
    int h = 0;
    SDL_GetWindowSize(window, &w, &h);
    if(w <= 0 || h <= 0) return SDL_HITTEST_NORMAL;
    const float x = static_cast<float>(p->x) * state->imguiSize.x / static_cast<float>(w);
    const float y = static_cast<float>(p->y) * state->imguiSize.y / static_cast<float>(h);
    for(const ImVec4& r : state->widgets)
        if(x >= r.x && x <= r.z && y >= r.y && y <= r.w) return SDL_HITTEST_NORMAL;
    return SDL_HITTEST_DRAGGABLE;
}

} // namespace

int main(int argc, char** argv) {
    int posX = -1;
    int posY = -1;
    bool startClickThrough = false;
    for(int i = 1; i < argc; ++i) {
        if(std::strcmp(argv[i], "--pos") == 0 && i + 2 < argc) {
            posX = std::atoi(argv[++i]);
            posY = std::atoi(argv[++i]);
        } else if(std::strcmp(argv[i], "--click-through") == 0) {
            startClickThrough = true;
        }
    }

    smg::GuiConfig config;
    config.title = "smg overlay";
    config.size = { 380, 220 };
    config.transparent = true;
    config.borderless = true;
    config.always_on_top = true;
    smg::GuiBase gui({ argc, argv }, config);

    if(posX < 0) {
        // top-right of the primary display
        SDL_Rect usable;
        SDL_GetDisplayUsableBounds(0, &usable);
        int w = 0;
        int h = 0;
        SDL_GetWindowSize(gui.get_window(), &w, &h);
        posX = usable.x + usable.w - w - 24;
        posY = usable.y + 24;
    }
    gui.set_window_position(posX, posY);
    if(startClickThrough) gui.set_click_through(true);

#if defined(_WIN32)
    const bool hotkey = RegisterHotKey(nullptr, HotkeyId, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'O') != 0;
    SDL_SetWindowsMessageHook(messageHook, nullptr);
#else
    const bool hotkey = false;
#endif

    CpuLoad cpu;
    cpu.sample();
    std::vector<float> history;
    std::vector<float> xs;
    float load = 0.0f;
    float accum = 0.0f;
    DragState drag;
    SDL_SetWindowHitTest(gui.get_window(), hitTest, &drag);

    gui.add_callback([&]() {
#if defined(_WIN32)
        if(g_hotkeyPressed) {
            g_hotkeyPressed = false;
            gui.set_click_through(!gui.click_through());
        }
#endif
        accum += gui.dt();
        if(accum >= 0.5f) {
            accum = 0.0f;
            load = cpu.sample();
            history.push_back(load);
            if(history.size() > HistoryLength) history.erase(history.begin());
            xs.resize(history.size());
            for(std::size_t i = 0; i < xs.size(); ++i) xs[i] = static_cast<float>(i) - static_cast<float>(xs.size() - 1);
        }

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowBgAlpha(gui.transparent() ? 0.55f : 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
        ImGui::Begin("##overlay", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking);
        ImGui::PopStyleVar();

        const std::time_t now = std::time(nullptr);
        char clock[16];
        std::strftime(clock, sizeof(clock), "%H:%M:%S", std::localtime(&now));
        ImGui::SetWindowFontScale(2.0f);
        ImGui::TextUnformatted(clock);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::SameLine(ImGui::GetWindowWidth() - 36.0f);
        if(ImGui::SmallButton("x")) gui.exit();
        drag.widgets.clear();
        drag.widgets.push_back(ImVec4(ImGui::GetItemRectMin().x, ImGui::GetItemRectMin().y, ImGui::GetItemRectMax().x, ImGui::GetItemRectMax().y));

        ImGui::Text("CPU %3.0f %%   %.0f fps", load, ImGui::GetIO().Framerate);
        ImPlot::PushStyleColor(ImPlotCol_FrameBg, ImVec4(0, 0, 0, 0));
        ImPlot::PushStyleColor(ImPlotCol_PlotBg, ImVec4(0, 0, 0, 0));
        if(ImPlot::BeginPlot("##cpu", ImVec2(-1, 80),
               ImPlotFlags_NoTitle | ImPlotFlags_NoLegend | ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect | ImPlotFlags_NoMouseText
                   | ImPlotFlags_NoInputs | ImPlotFlags_NoFrame)) {
            ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
            ImPlot::SetupAxisLimits(ImAxis_X1, -(HistoryLength - 1), 0.0, ImPlotCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, 100.0, ImPlotCond_Always);
            ImPlot::SetNextFillStyle(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), 0.35f);
            ImPlot::PlotShaded("cpu", xs.data(), history.data(), static_cast<int>(history.size()));
            ImPlot::SetNextLineStyle(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), 2.0f);
            ImPlot::PlotLine("cpu", xs.data(), history.data(), static_cast<int>(history.size()));
            ImPlot::EndPlot();
        }
        ImPlot::PopStyleColor(2);

        bool clickThrough = gui.click_through();
        if(ImGui::Checkbox("click-through", &clickThrough)) gui.set_click_through(clickThrough);
        drag.widgets.push_back(ImVec4(ImGui::GetItemRectMin().x, ImGui::GetItemRectMin().y, ImGui::GetItemRectMax().x, ImGui::GetItemRectMax().y));
        drag.imguiSize = ImGui::GetIO().DisplaySize;
        ImGui::SameLine();
        ImGui::TextDisabled(hotkey ? "Ctrl+Alt+O toggles" : "(no global hotkey here)");

        ImGui::End();
        return 0;
    });

    while(gui.mainLoopIteration()) {}

#if defined(_WIN32)
    if(hotkey) UnregisterHotKey(nullptr, HotkeyId);
#endif
    return 0;
}
