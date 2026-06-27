// gui smoke test: build a renderer + sheet and issue one draw to the default framebuffer
#include "Camera.hh"
#include "GuiBase.hh"
#include "SpriteRenderer.hh"
#include "SpriteSheet.hh"

#include <Corrade/Containers/ArrayView.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace smg;

class RendererTest: public GuiBase {
public:
    explicit RendererTest(const Arguments& arguments) : GuiBase(arguments) {
        std::array<unsigned char, 4> px{ 255, 255, 255, 255 };
        ShSpriteSheetPr sheet =
            SpriteSheet::from_pixels(Corrade::Containers::arrayView(reinterpret_cast<const char*>(px.data()), px.size()),
                Magnum::Vector2i{ 1, 1 },
                Magnum::PixelFormat::RGBA8Unorm,
                1,
                1);

        std::vector<Sprite> sprites;
        sprites.push_back(Sprite{ sheet, 0, Magnum::Vector3{ 0.0f }, SpriteParams{} });

        Camera cam;
        cam.iso();
        SpriteRenderer renderer;
        renderer.draw(sprites, cam.view(), cam.projection(1.0f)); // must not crash
        std::printf("OK sprite renderer draw\n");
        std::exit(0);
    }
};

MAGNUM_APPLICATION_MAIN(RendererTest)
