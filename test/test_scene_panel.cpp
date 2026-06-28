// interactive smoke test: a scene panel renders cube/sphere/grid/axes.
// run manually (opens a window); registered with ctest like the other GUI tests.
#include "GuiBase.hh"
#include "ScenePanel.hh"

#include <Magnum/Math/Matrix4.h>

using namespace Magnum;
using namespace smg;

int panel_cb(void* data) {
    reinterpret_cast<ScenePanel*>(data)->draw("smoke", Vector2i{ 640, 480 });
    return 0;
}

int main(int argc, char** argv) {
    GuiBase gui({ argc, argv });

    ScenePanel panel;
    panel.add_axes();
    panel.add_cube();
    panel.add_sphere(Matrix4::translation({ 2.0f, 0.0f, 0.0f }));
    panel.add_grid();

    ShDrawCallbackPr cb = DrawCallback::create();
    cb->set_callback(panel_cb);
    cb->set_data(&panel);
    gui.add_callback(cb);

    bool done = false;
    while(!done) done = !gui.mainLoopIteration();
    gui.exit();
}
