// scene_panel example: orbit a cube + sphere + grid + axes
#include "GuiBase.hh"
#include "ScenePanel.hh"

#include <Magnum/Math/Matrix4.h>

using namespace Magnum;
using namespace smg;

int scene_callback(ScenePanel& panel) {
    panel.draw("3D Scene", Vector2i{ 800, 600 });
    return 0;
}

class SceneExample: public GuiBase {
public:
    explicit SceneExample(const Arguments& arguments) : GuiBase(arguments) {
        _panel = ScenePanel::create();
        _panel->add_grid();
        _panel->add_axes(1.5f);
        _panel->add_cube(Matrix4::translation({ -1.5f, 0.5f, 0.0f }), Color3{ 0.9f, 0.4f, 0.3f });
        _panel->add_sphere(Matrix4::translation({ 1.5f, 0.5f, 0.0f }), Color3{ 0.3f, 0.6f, 0.9f });

        // overbright sphere so the bloom glow is clearly visible
        _panel->add_sphere(Matrix4::translation({ 0.0f, 1.6f, 0.0f }) * Matrix4::scaling(Vector3{ 0.4f }), Color3{ 3.0f });

        add_callback([this]() { return scene_callback(*_panel); });
    }

private:
    ShScenePanelPr _panel;
};

MAGNUM_APPLICATION_MAIN(SceneExample)
