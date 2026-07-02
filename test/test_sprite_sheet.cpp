// gui smoke test: build a sheet from raw pixels under a real GL context
#include "GuiBase.hh"
#include "SpriteSheet.hh"

#include <Corrade/Containers/ArrayView.h>
#include <array>
#include <cstdio>
#include <cstdlib>

using namespace smg;

class SheetTest: public GuiBase {
public:
    explicit SheetTest(const Arguments& arguments) : GuiBase(arguments) {
        // 2x2 RGBA white
        std::array<unsigned char, 2 * 2 * 4> px{};
        px.fill(255);
        ShSpriteSheetPr sheet =
            SpriteSheet::from_pixels(Corrade::Containers::arrayView(reinterpret_cast<const char*>(px.data()), px.size()),
                Magnum::Vector2i{ 2, 2 },
                Magnum::PixelFormat::RGBA8Unorm,
                /*cols*/ 2,
                /*rows*/ 1);
        int rc = 0;
        if(sheet->grid().count() != 2) {
            std::printf("FAIL sheet count\n");
            rc = 1;
        }
        if(sheet->texture().id() == 0) {
            std::printf("FAIL texture id\n");
            rc = 1;
        }
        std::exit(rc); // smoke test: construct, validate, exit
    }
};

MAGNUM_APPLICATION_MAIN(SheetTest)
