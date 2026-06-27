// gui smoke test: ScenePanel renders with the bloom glow pass active.
// Bloom is on by default (desktop), so draw() exercises the full
// scene -> render_bloom_texture -> render_final -> ImGui path.
// Needs a display; registered under the "gui" label (opt-in) like the others.
#include "GuiBase.hh"
#include "ScenePanel.hh"

#include <Magnum/Math/Matrix4.h>

using namespace Magnum;
using namespace smg;

int bloom_cb(void* data) {
    reinterpret_cast<ScenePanel*>(data)->draw("bloom smoke", Vector2i{ 640, 480 });
    return 0;
}

int main(int argc, char** argv) {
    GuiBase gui({ argc, argv });

    ScenePanel panel;
    panel.add_grid();
    panel.add_axes();
    // overbright sphere drives a visible glow when bloom is active
    panel.add_sphere(Matrix4::scaling(Vector3{ 0.5f }), Color3{ 3.0f });

    ShDrawCallbackPr cb = DrawCallback::create();
    cb->set_callback(bloom_cb);
    cb->set_data(&panel);
    gui.add_callback(cb);

    bool done = false;
    while(!done) done = !gui.mainLoopIteration();
    gui.exit();
}
