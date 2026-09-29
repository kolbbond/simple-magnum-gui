// gui smoke test: ScenePanel renders with the bloom glow pass active.
// Bloom is on by default (desktop), so draw() exercises the full
// scene -> render_bloom_texture -> render_final -> ImGui path.
// Needs a display; registered under the "gui" label (opt-in) like the others.
#include "GuiBase.hh"
#include "ScenePanel.hh"
#include "gui_test_util.hh"

#include <Magnum/Math/Matrix4.h>

using namespace Magnum;
using namespace smg;

int bloom_cb(ScenePanel& panel) {
    panel.draw("bloom smoke", Vector2i{ 640, 480 });
    return 0;
}

int main(int argc, char** argv) {
    GuiBase gui({ argc, argv });
    smgtest::frame_limit(gui);

    ScenePanel panel;
    panel.add_grid();
    panel.add_axes();
    // overbright sphere drives a visible glow when bloom is active
    panel.add_sphere(Matrix4::scaling(Vector3{ 0.5f }), Color3{ 3.0f });

    gui.add_callback([&panel]() { return bloom_cb(panel); });

    bool done = false;
    while(!done) done = !gui.mainLoopIteration();
    gui.exit();
}
