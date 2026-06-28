// gui smoke test: render one frame of a 3D plot, then exit
#include "GuiBase.hh"
#include "Plot3D.hh"

#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace smg;

static std::vector<float> g_x, g_y, g_z;

int plot3d_cb(void* /*data*/) {
    ImGui::Begin("Plot3D");
    if(Plot3D p{ "orbit" }) {
        p.line("path", g_x, g_y, g_z);
        p.scatter("pts", g_x, g_y, g_z);
    }
    ImGui::End();
    std::printf("OK plot3d frame\n");
    std::exit(0);
    return 0;
}

class Plot3DTest: public GuiBase {
public:
    explicit Plot3DTest(const Arguments& arguments) : GuiBase(arguments) {
        for(int i = 0; i < 16; ++i) {
            g_x.push_back(float(i));
            g_y.push_back(float(i) * 0.5f);
            g_z.push_back(float(i) * 0.25f);
        }
        _cb = DrawCallback::create();
        _cb->set_callback(plot3d_cb);
        add_callback(_cb);
    }

private:
    ShDrawCallbackPr _cb;
};

MAGNUM_APPLICATION_MAIN(Plot3DTest)
