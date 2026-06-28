// gui smoke test: load a PNG into a SpriteSheet via the optional importer path
#include "GuiBase.hh"
#include "SpriteSheet.hh"

#include <cstdio>
#include <cstdlib>

using namespace smg;

class LoadTest: public GuiBase {
public:
    explicit LoadTest(const Arguments& arguments) : GuiBase(arguments) {
        ShSpriteSheetPr sheet = SpriteSheet::load(SMG_TEST_ASSET_DIR "/../assets/smg_example.png", 1, 1);
        if(!sheet) {
            std::printf("FAIL load returned null\n");
            std::exit(1);
        }
        if(sheet->texture().id() == 0) {
            std::printf("FAIL texture id\n");
            std::exit(1);
        }
        std::printf("OK sprite load\n");
        std::exit(0);
    }
};

MAGNUM_APPLICATION_MAIN(LoadTest)
