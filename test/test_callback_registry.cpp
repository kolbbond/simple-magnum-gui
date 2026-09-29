// gui test: add/remove/clear callbacks across real frames, incl. removal mid-dispatch
#include "GuiBase.hh"
#include "test_util.hh"

#include <cstdlib>

int main(int argc, char** argv) {
    smg::GuiBase gui({ argc, argv });

    int a = 0;
    int b = 0;
    const smg::ShDrawCallbackPr ha = gui.add_callback([&a]() {
        ++a;
        return 0;
    });
    gui.add_callback(smg::DrawCallback::create([&b]() {
        ++b;
        return 0;
    }));
    gui.add_callback(smg::ShDrawCallbackPr{}); // ignored, must not crash dispatch

    gui.mainLoopIteration();
    CHECK(a == 1);
    CHECK(b == 1);

    CHECK(gui.remove_callback(ha));
    CHECK(!gui.remove_callback(ha));
    gui.mainLoopIteration();
    CHECK(a == 1);
    CHECK(b == 2);

    // one-shot: removes itself during dispatch; the snapshot keeps it alive until done
    int once = 0;
    smg::ShDrawCallbackPr self;
    self = gui.add_callback([&]() {
        ++once;
        gui.remove_callback(self);
        self.reset();
        return 0;
    });
    gui.mainLoopIteration();
    gui.mainLoopIteration();
    CHECK(once == 1);

    gui.clear_callbacks();
    gui.mainLoopIteration();
    CHECK(b == 4);

    gui.exit();
    std::exit(smgtest::failures() ? 1 : 0);
}
