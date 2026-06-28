// sprite_panel example: a grid of billboard sprites under an iso camera, plus an additive glow
#include "GuiBase.hh"
#include "IsoGrid.hh"
#include "ScenePanel.hh"
#include "SpriteSheet.hh"

#include <Corrade/Containers/ArrayView.h>
#include <array>

using namespace Magnum;
using namespace smg;

// build a tiny solid-color RGBA sheet at runtime (stand-in for a prerendered atlas)
static ShSpriteSheetPr solid_sheet(unsigned char r, unsigned char g, unsigned char b) {
    std::array<unsigned char, 4> px{ r, g, b, 255 };
    return SpriteSheet::from_pixels(Corrade::Containers::arrayView(reinterpret_cast<const char*>(px.data()), px.size()),
        Vector2i{ 1, 1 },
        PixelFormat::RGBA8Unorm,
        1,
        1);
}

int sprite_callback(void* data) {
    reinterpret_cast<ScenePanel*>(data)->draw("Iso Sprites", Vector2i{ 800, 600 });
    return 0;
}

class SpriteExample: public GuiBase {
public:
    explicit SpriteExample(const Arguments& arguments) : GuiBase(arguments) {
        _panel = ScenePanel::create();
        _panel->add_grid();
        _panel->camera().iso();

        ShSpriteSheetPr unit = solid_sheet(220, 90, 70);
        IsoGrid grid{ 1.0f };
        for(int j = -2; j <= 2; ++j)
            for(int i = -2; i <= 2; ++i) {
                SpriteParams p;
                p.size = Vector2{ 0.8f, 1.2f };
                _panel->add_sprite(unit, 0, grid.to_world({ i, j }), p);
            }

        // an additive glow sprite (pairs with bloom when SMG_WITH_BLOOM is on)
        ShSpriteSheetPr glow = solid_sheet(120, 160, 255);
        SpriteParams gp;
        gp.size = Vector2{ 1.5f, 1.5f };
        gp.anchor = Anchor::Center;
        gp.blend = SpriteBlend::Additive;
        gp.tint = Color4{ 1.0f, 1.0f, 1.0f, 0.6f };
        _panel->add_sprite(glow, 0, grid.to_world({ 0, 0 }) + Vector3{ 0.0f, 1.0f, 0.0f }, gp);

        _cb = DrawCallback::create();
        _cb->set_callback(sprite_callback);
        _cb->set_data(_panel.get());
        add_callback(_cb);
    }

private:
    ShScenePanelPr _panel;
    ShDrawCallbackPr _cb;
};

MAGNUM_APPLICATION_MAIN(SpriteExample)
