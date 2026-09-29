// gui smoke test: open an smg::FileDialog and pump frames through draw().
// Needs a display; registered under the "gui" label (opt-in) like the others.
#include "FileDialog.hh"
#include "GuiBase.hh"
#include "gui_test_util.hh"

using namespace smg;

struct State {
    FileDialog dialog;
};

int fd_cb(State& state) {
    state.dialog.draw();
    return 0;
}

int main(int argc, char** argv) {
    GuiBase gui({ argc, argv });
    smgtest::frame_limit(gui);

    State state;
    state.dialog.open("smoke");

    gui.add_callback([&state]() { return fd_cb(state); });

    bool done = false;
    while(!done) done = !gui.mainLoopIteration();
    gui.exit();
}
