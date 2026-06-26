// gui smoke test: open an smg::FileDialog and pump frames through draw().
// Needs a display; registered under the "gui" label (opt-in) like the others.
#include "FileDialog.hh"
#include "GuiBase.hh"

using namespace smg;

struct State {
    FileDialog dialog;
};

int fd_cb(void* data) {
    reinterpret_cast<State*>(data)->dialog.draw();
    return 0;
}

int main(int argc, char** argv) {
    GuiBase gui({ argc, argv });

    State state;
    state.dialog.open("smoke");

    ShDrawCallbackPr cb = DrawCallback::create();
    cb->set_callback(fd_cb);
    cb->set_data(&state);
    gui.add_callback(cb);

    bool done = false;
    while(!done) done = !gui.mainLoopIteration();
    gui.exit();
}
