// interactive smoke test: a scene panel renders cube/sphere/grid/axes.
// run manually (opens a window); registered with ctest like the other GUI tests.
#include "GuiBase.hh"
#include "ScenePanel.hh"
#include "gui_test_util.hh"

#include <Magnum/Math/Matrix4.h>

using namespace Magnum;
using namespace smg;

int panel_cb(ScenePanel& panel) {
    panel.draw("smoke", Vector2i{ 640, 480 });
    return 0;
}

int main(int argc, char** argv) {
    GuiBase gui({ argc, argv });
    smgtest::frame_limit(gui);

    ScenePanel panel;
    panel.add_axes();
    panel.add_cube();
    panel.add_sphere(Matrix4::translation({ 2.0f, 0.0f, 0.0f }));
    panel.add_grid();

    gui.add_callback([&panel]() { return panel_cb(panel); });

    bool done = false;
    while(!done) done = !gui.mainLoopIteration();
    gui.exit();
}
