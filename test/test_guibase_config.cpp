// gui test: GuiConfig is applied, and --smg-frames / max_frames end the loop on their own
#include "GuiBase.hh"
#include "test_util.hh"

#include <cstdlib>

namespace {

long run_until_exit(smg::GuiBase& gui) {
    long frames = 0;
    gui.add_callback([&frames]() {
        ++frames;
        return 0;
    });
    // bounded so a broken frame limit fails instead of hanging
    for(int i = 0; i < 100 && gui.mainLoopIteration(); ++i) {}
    return frames;
}

} // namespace

int main(int argc, char** argv) {
    {
        // the flag must also get past Magnum's own argument parser
        char flag[] = "--smg-frames";
        char value[] = "3";
        char* args[] = { argv[0], flag, value, nullptr };
        int n = 3;
        smg::GuiConfig config;
        config.title = "config test";
        config.size = { 640, 480 };
        config.samples = 0;
        smg::GuiBase gui({ n, args }, config);
        CHECK(gui.windowSize() == Magnum::Vector2i(640, 480));
        CHECK(run_until_exit(gui) == 3);
    }
    {
        smg::GuiConfig config;
        config.max_frames = 2;
        int n = 1;
        smg::GuiBase gui({ n, argv }, config);
        CHECK(run_until_exit(gui) == 2);
    }
    (void)argc;
    std::exit(smgtest::failures() ? 1 : 0);
}
