// gui test: the section plane discards the clipped half-space and shows the
// cut interior as the flat cap colour
#include <Corrade/Containers/StridedArrayView.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Framebuffer.h>
#include <Magnum/Image.h>
#include <Magnum/Math/Color.h>
#include <Magnum/PixelFormat.h>

#include "GuiBase.hh"
#include "ScenePanel.hh"
#include "test_util.hh"

#include <cstdlib>
#include <memory>

namespace {

// exposes the pre-bloom scene target for readback
class ProbePanel: public smg::ScenePanel {
public:
    Magnum::Color3ub center() {
        const Magnum::Vector2i c = _fbo_size / 2;
        const Magnum::Image2D img = _fbo.read({ c, c + Magnum::Vector2i{ 1 } }, { Magnum::PixelFormat::RGBA8Unorm });
        return img.pixels<Magnum::Color4ub>()[0][0].rgb();
    }
};

bool near(const Magnum::Color3ub& a, const Magnum::Color3& b) {
    const Magnum::Color3ub e = Magnum::Math::pack<Magnum::Color3ub>(b);
    for(int i = 0; i < 3; ++i)
        if(std::abs(int(a[i]) - int(e[i])) > 2) return false;
    return true;
}

void check_equation() {
    smg::SectionPlane p;
    CHECK(p.equation() == (Magnum::Vector4{ 0.0f, 0.0f, 0.0f, -1.0f }));
    p.enabled = true;
    p.normal = { 0.0f, 2.0f, 0.0f };
    p.offset = 0.5f;
    CHECK(p.equation() == (Magnum::Vector4{ 0.0f, 1.0f, 0.0f, -0.5f }));
    p.normal = {};
    CHECK(p.equation() == (Magnum::Vector4{ 0.0f, 0.0f, 0.0f, -1.0f }));
}

} // namespace

int main(int argc, char** argv) {
    check_equation();

    smg::GuiBase gui({ argc, argv });
    std::shared_ptr<ProbePanel> panel = std::make_shared<ProbePanel>();
    panel->add_cube(); // [-1, 1]^3; fit() centres the pivot, so the image centre ray hits the origin
    panel->set_bloom_enabled(false);

    const Magnum::Color3 clear{ 0.12f, 0.12f, 0.14f };
    const Magnum::Color3 cap = panel->section().cap_color;
    int frame = 0;
    gui.add_callback([&]() {
        smg::SectionPlane& s = panel->section();
        const Magnum::Vector3 to_eye = (panel->camera().eye() - panel->camera().pivot()).normalized();
        if(frame == 1) {
            // remove the camera-side half: the centre ray sees the interior
            s.enabled = true;
            s.normal = to_eye;
            s.offset = -0.5f;
        } else if(frame == 2) {
            s.offset = -5.0f; // whole cube clipped
        }

        panel->draw("section", { 200, 150 });
        const Magnum::Color3ub px = panel->center();
        if(frame == 0) CHECK(!near(px, cap) && !near(px, clear));
        if(frame == 1) CHECK(near(px, cap));
        if(frame == 2) CHECK(near(px, clear));

        Magnum::GL::defaultFramebuffer.bind();
        if(++frame == 3) gui.exit();
        return 0;
    });

    while(gui.mainLoopIteration()) {}
    std::exit(smgtest::failures() ? 1 : 0);
}
