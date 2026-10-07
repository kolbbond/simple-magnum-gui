// gui test: ScenePanel::draw leaves the caller's GL state (target, viewport, caps,
// blend, clear values) as it found it, with and without bloom
#include <Magnum/GL/Framebuffer.h>
#include <Magnum/GL/OpenGL.h>
#include <Magnum/GL/Renderbuffer.h>
#include <Magnum/GL/RenderbufferFormat.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/Math/Color.h>

#include "GuiBase.hh"
#include "ScenePanel.hh"
#include "test_util.hh"

#include <cstdlib>

namespace {

void check_consumer_state(GLint fbo) {
    GLint bound = -1;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &bound);
    CHECK(bound == fbo);
    GLint vp[4] = {};
    glGetIntegerv(GL_VIEWPORT, vp);
    CHECK(vp[0] == 3 && vp[1] == 5 && vp[2] == 70 && vp[3] == 90);
    CHECK(glIsEnabled(GL_BLEND) == GL_TRUE);
    CHECK(glIsEnabled(GL_DEPTH_TEST) == GL_FALSE);
    CHECK(glIsEnabled(GL_CULL_FACE) == GL_TRUE);
    GLint src = 0;
    GLint dst = 0;
    glGetIntegerv(GL_BLEND_SRC_RGB, &src);
    glGetIntegerv(GL_BLEND_DST_RGB, &dst);
    CHECK(src == GL_ONE && dst == GL_ONE);
    GLfloat clear[4] = {};
    glGetFloatv(GL_COLOR_CLEAR_VALUE, clear);
    CHECK(smgtest::approx(clear[0], 0.25f) && smgtest::approx(clear[3], 0.5f));
}

} // namespace

int main(int argc, char** argv) {
    smg::GuiBase gui({ argc, argv });

    Magnum::GL::Renderbuffer color;
    color.setStorage(Magnum::GL::RenderbufferFormat::RGBA8, { 128, 128 });
    Magnum::GL::Framebuffer consumer{ { {}, { 128, 128 } } };
    consumer.attachRenderbuffer(Magnum::GL::Framebuffer::ColorAttachment{ 0 }, color);

    smg::ShScenePanelPr panel = smg::ScenePanel::create();
    panel->add_cube();

    int frame = 0;
    gui.add_callback([&]() {
        // a consumer mid-pass: its own target and deliberately odd state
        consumer.bind();
        consumer.setViewport({ { 3, 5 }, { 73, 95 } });
        Magnum::GL::Renderer::enable(Magnum::GL::Renderer::Feature::Blending);
        Magnum::GL::Renderer::disable(Magnum::GL::Renderer::Feature::DepthTest);
        Magnum::GL::Renderer::enable(Magnum::GL::Renderer::Feature::FaceCulling);
        Magnum::GL::Renderer::setBlendFunction(Magnum::GL::Renderer::BlendFunction::One, Magnum::GL::Renderer::BlendFunction::One);
        Magnum::GL::Renderer::setClearColor(Magnum::Color4{ 0.25f, 0.0f, 0.0f, 0.5f });

        panel->set_bloom_enabled(frame == 1 && smg::ScenePanel::bloom_available());
        panel->draw("gl state", { 200, 150 });
        check_consumer_state(GLint(consumer.id()));

        // hand the default target back for GuiBase's own ImGui pass
        Magnum::GL::defaultFramebuffer.bind();
        if(++frame == 2) gui.exit();
        return 0;
    });

    while(gui.mainLoopIteration()) {}
    std::exit(smgtest::failures() ? 1 : 0);
}
