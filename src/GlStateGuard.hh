// internal: snapshot/restore the GL state an offscreen panel pass clobbers, so a
// consumer drawing a panel from inside its own render pass keeps its target/state
#pragma once

#include <array>

#include <Magnum/GL/Context.h>
#include <Magnum/GL/OpenGL.h>

namespace smg {

class GlStateGuard {
public:
    GlStateGuard() {
#ifdef MAGNUM_TARGET_GLES2
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &_draw_fbo);
#else
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &_draw_fbo);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &_read_fbo);
#endif
        glGetIntegerv(GL_VIEWPORT, _viewport.data());
        _depth_test = glIsEnabled(GL_DEPTH_TEST);
        _cull_face = glIsEnabled(GL_CULL_FACE);
        _blend = glIsEnabled(GL_BLEND);
        _scissor = glIsEnabled(GL_SCISSOR_TEST);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &_depth_mask);
        glGetIntegerv(GL_BLEND_SRC_RGB, &_blend_src_rgb);
        glGetIntegerv(GL_BLEND_DST_RGB, &_blend_dst_rgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &_blend_src_alpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &_blend_dst_alpha);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &_blend_eq_rgb);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &_blend_eq_alpha);
        glGetFloatv(GL_COLOR_CLEAR_VALUE, _clear_color.data());
        glGetFloatv(GL_DEPTH_CLEAR_VALUE, &_clear_depth);
    }

    ~GlStateGuard() {
#ifdef MAGNUM_TARGET_GLES2
        glBindFramebuffer(GL_FRAMEBUFFER, GLuint(_draw_fbo));
#else
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, GLuint(_draw_fbo));
        glBindFramebuffer(GL_READ_FRAMEBUFFER, GLuint(_read_fbo));
#endif
        glViewport(_viewport[0], _viewport[1], _viewport[2], _viewport[3]);
        set(GL_DEPTH_TEST, _depth_test);
        set(GL_CULL_FACE, _cull_face);
        set(GL_BLEND, _blend);
        set(GL_SCISSOR_TEST, _scissor);
        glDepthMask(_depth_mask);
        glBlendFuncSeparate(GLenum(_blend_src_rgb), GLenum(_blend_dst_rgb), GLenum(_blend_src_alpha), GLenum(_blend_dst_alpha));
        glBlendEquationSeparate(GLenum(_blend_eq_rgb), GLenum(_blend_eq_alpha));
        glClearColor(_clear_color[0], _clear_color[1], _clear_color[2], _clear_color[3]);
#ifdef MAGNUM_TARGET_GLES
        glClearDepthf(_clear_depth);
#else
        glClearDepth(_clear_depth);
#endif
        // the raw calls above bypass Magnum's tracker; drop its cached bindings
        Magnum::GL::Context::current().resetState(Magnum::GL::Context::State::Framebuffers | Magnum::GL::Context::State::Renderer);
    }

    GlStateGuard(const GlStateGuard&) = delete;
    GlStateGuard& operator=(const GlStateGuard&) = delete;

private:
    static void set(GLenum cap, GLboolean on) {
        if(on == GL_TRUE)
            glEnable(cap);
        else
            glDisable(cap);
    }

    GLint _draw_fbo = 0;
    GLint _read_fbo = 0;
    std::array<GLint, 4> _viewport{};
    GLboolean _depth_test = GL_FALSE;
    GLboolean _cull_face = GL_FALSE;
    GLboolean _blend = GL_FALSE;
    GLboolean _scissor = GL_FALSE;
    GLboolean _depth_mask = GL_TRUE;
    GLint _blend_src_rgb = 0;
    GLint _blend_dst_rgb = 0;
    GLint _blend_src_alpha = 0;
    GLint _blend_dst_alpha = 0;
    GLint _blend_eq_rgb = 0;
    GLint _blend_eq_alpha = 0;
    std::array<GLfloat, 4> _clear_color{};
    GLfloat _clear_depth = 1.0f;
};

} // namespace smg
