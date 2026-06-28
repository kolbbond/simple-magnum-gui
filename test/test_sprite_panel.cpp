// gui smoke test: sprites placed on an iso grid render through ScenePanel
#include "GuiBase.hh"
#include "IsoGrid.hh"
#include "ScenePanel.hh"
#include "SpriteSheet.hh"

#include <Corrade/Containers/ArrayView.h>
#include <array>
#include <cstdio>
#include <cstdlib>

using namespace smg;

// render one integrated frame (meshes + sprite pass), then exit cleanly
int sprite_cb(void* data) {
    reinterpret_cast<ScenePanel*>(data)->draw("Sprites", Magnum::Vector2i{ 320, 240 });
    std::printf("OK sprite panel frame\n");
    std::exit(0);
    return 0;
}

class SpritePanelTest: public GuiBase {
public:
    explicit SpritePanelTest(const Arguments& arguments) : GuiBase(arguments) {
        _panel = ScenePanel::create();
        _panel->camera().iso();

        std::array<unsigned char, 4> px{ 255, 255, 255, 255 };
        ShSpriteSheetPr sheet =
            SpriteSheet::from_pixels(Corrade::Containers::arrayView(reinterpret_cast<const char*>(px.data()), px.size()),
                Magnum::Vector2i{ 1, 1 },
                Magnum::PixelFormat::RGBA8Unorm,
                1,
                1);

        IsoGrid grid{ 1.0f };
        const std::size_t h = _panel->add_sprite(sheet, 0, grid.to_world({ 0, 0 }), SpriteParams{});
        if(_panel->sprite(h).frame != 0) {
            std::printf("FAIL sprite handle\n");
            std::exit(1);
        }

        _cb = DrawCallback::create();
        _cb->set_callback(sprite_cb);
        _cb->set_data(_panel.get());
        add_callback(_cb);
    }

private:
    ShScenePanelPr _panel;
    ShDrawCallbackPr _cb;
};

MAGNUM_APPLICATION_MAIN(SpritePanelTest)
