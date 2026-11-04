// scripted mouse input on a ScenePanel: a drag on the image orbits the camera without moving the host window,
// and a drag on the title bar moves the window without touching the camera.
#include "GuiBase.hh"
#include "ScenePanel.hh"

#include <imgui.h>

#include <cmath>
#include <cstdio>

using namespace Magnum;
using namespace smg;

namespace {

struct Drag {
    ImVec2 from, to;
};

} // namespace

int main(int argc, char** argv) {
    GuiConfig config;
    config.size = { 800, 600 };
    GuiBase gui({ argc, argv }, config);

    ScenePanel panel;
    panel.add_cube();

    // events queued in frame f are seen from frame f + 1
    enum Phase { Settle, ImageDrag, TitleDrag, Done };
    Phase phase = Settle;
    int step = 0;
    ImVec2 win{}, win_before{};
    Vector3 eye_before;
    bool image_moved_window = false, image_orbited = false, title_moved_window = false, title_orbited = false;

    auto run_drag = [&](const Drag& d) {
        ImGuiIO& io = ImGui::GetIO();
        constexpr int n = 8;
        if(step == 0) io.AddMousePosEvent(d.from.x, d.from.y);
        else if(step == 1) io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
        else if(step < 2 + n) {
            const float t = float(step - 1) / n;
            io.AddMousePosEvent(d.from.x + t * (d.to.x - d.from.x), d.from.y + t * (d.to.y - d.from.y));
        } else if(step == 2 + n) io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
        return ++step > 4 + n;
    };

    gui.add_callback([&]() {
        ImGui::SetNextWindowPos({ 40, 40 }, ImGuiCond_FirstUseEver);
        panel.draw("input", Vector2i{ 320, 240 });
        ImGui::Begin("input"); // append: read back where the panel's window is
        win = ImGui::GetWindowPos();
        ImGui::End();

        switch(phase) {
        case Settle:
            if(++step > 3) phase = ImageDrag, step = 0, win_before = win, eye_before = panel.camera().eye();
            break;
        case ImageDrag: {
            const ImVec2 c{ win.x + 170, win.y + 160 }; // inside the image
            if(run_drag({ c, { c.x + 80, c.y + 30 } })) {
                image_moved_window = win.x != win_before.x || win.y != win_before.y;
                image_orbited = (panel.camera().eye() - eye_before).length() > 1e-4f;
                phase = TitleDrag, step = 0, win_before = win, eye_before = panel.camera().eye();
            }
            break;
        }
        case TitleDrag: {
            const ImVec2 t{ win_before.x + 60, win_before.y + 8 }; // title bar
            if(run_drag({ t, { t.x + 90, t.y + 50 } })) {
                title_moved_window = win.x != win_before.x || win.y != win_before.y;
                title_orbited = (panel.camera().eye() - eye_before).length() > 1e-4f;
                phase = Done;
            }
            break;
        }
        case Done: gui.exit(); break;
        }
        return 0;
    });

    while(phase != Done && gui.mainLoopIteration()) {}

    std::printf("image drag: orbited %d, moved window %d | title drag: moved window %d, orbited %d\n", image_orbited,
        image_moved_window, title_moved_window, title_orbited);
    const bool ok = image_orbited && !image_moved_window && title_moved_window && !title_orbited;
    return ok ? 0 : 1;
}
