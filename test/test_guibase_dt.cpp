// gui smoke test: dt() is finite, non-negative, and clamped after a few frames
#include "GuiBase.hh"

#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv) {
    smg::GuiBase gui({ argc, argv });
    for(int i = 0; i < 3; ++i) gui.mainLoopIteration();
    const float d = gui.dt();
    std::printf("dt = %f\n", d);
    if(!(d >= 0.0f && d <= 0.1f)) {
        std::printf("FAIL dt out of range\n");
        gui.exit();
        std::exit(1);
    }
    gui.exit();
    std::exit(0);
}
