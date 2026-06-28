// gui smoke test: render one frame of 2D plots, then exit
#include "GuiBase.hh"
#include "Plot.hh"

#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace smg;

static std::vector<float> g_x, g_y;

int plot_cb(void* /*data*/) {
    ImGui::Begin("Plots");
    if(Plot p{ "signals" }) {
        p.line("y", g_y);
        p.scatter("pts", g_x, g_y);
        p.bar("b", g_y);
    }
    smg::plot::line("quick", g_y);
    ImGui::End();
    std::printf("OK plot frame\n");
    std::exit(0);
    return 0;
}

class PlotTest: public GuiBase {
public:
    explicit PlotTest(const Arguments& arguments) : GuiBase(arguments) {
        for(int i = 0; i < 16; ++i) {
            g_x.push_back(float(i));
            g_y.push_back(float(i) * 0.5f);
        }
        _cb = DrawCallback::create();
        _cb->set_callback(plot_cb);
        add_callback(_cb);
    }

private:
    ShDrawCallbackPr _cb;
};

MAGNUM_APPLICATION_MAIN(PlotTest)
