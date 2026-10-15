// gui test: the section plane discards the clipped half-space, shows the
// cut interior as the flat cap colour, and culls sprites on the clipped side
#include <Corrade/Containers/StridedArrayView.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Framebuffer.h>
#include <Magnum/Image.h>
#include <Magnum/Math/Color.h>
#include <Magnum/PixelFormat.h>

#include "GuiBase.hh"
#include "ScenePanel.hh"
#include "SpriteSheet.hh"
#include "test_util.hh"

#include <array>
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
    // [-1, 1]^3; fit() centres the pivot, so the image centre ray hits the origin
    panel->add_cube({}, Magnum::Color3{ 1.0f, 0.0f, 0.0f });
    panel->set_bloom_enabled(false);

    std::shared_ptr<ProbePanel> sprites = std::make_shared<ProbePanel>();
    const std::array<unsigned char, 4> white{ 255, 255, 255, 255 };
    smg::SpriteParams centered;
    centered.anchor = smg::Anchor::Center;
    sprites->add_sprite(
        smg::SpriteSheet::from_pixels(Corrade::Containers::arrayView(reinterpret_cast<const char*>(white.data()), white.size()),
            Magnum::Vector2i{ 1 },
            Magnum::PixelFormat::RGBA8Unorm,
            1,
            1),
        0,
        Magnum::Vector3{ 0.0f },
        centered);
    sprites->set_bloom_enabled(false); // auto-fit must frame a lone sprite by its size

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

        if(frame == 0) panel->draw("section", { 0, 150 }); // collapsed host: must not abort
        panel->draw("section", { 200, 150 });
        const Magnum::Color3ub px = panel->center();
        // ambient and diffuse both carry the object colour: red stays red, not a grey floor
        if(frame == 0) CHECK(int(px.r()) > int(px.g()) + 40 && px.g() == px.b());
        if(frame == 1) CHECK(near(px, cap));
        if(frame == 2) CHECK(near(px, clear));

        smg::SectionPlane& ss = sprites->section();
        ss.enabled = frame == 1;
        ss.normal = (sprites->camera().eye() - sprites->camera().pivot()).normalized();
        ss.offset = -0.5f;
        sprites->draw("section sprites", { 200, 150 });
        const Magnum::Color3ub spx = sprites->center();
        if(frame == 0) CHECK(near(spx, Magnum::Color3{ 1.0f }));
        if(frame == 1) CHECK(near(spx, clear));

        Magnum::GL::defaultFramebuffer.bind();
        if(++frame == 3) gui.exit();
        return 0;
    });

    while(gui.mainLoopIteration()) {}
    std::exit(smgtest::failures() ? 1 : 0);
}
