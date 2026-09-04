#include "ScenePanel.hh"

#include <algorithm>

#include <Corrade/Utility/Assert.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/RenderbufferFormat.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/ImGuiIntegration/Integration.h>
#include <Magnum/ImGuiIntegration/Widgets.h>

#include <imgui.h>

#include "IsoGrid.hh"
#include "Primitives.hh"
#include "SpriteRenderer.hh"

#ifdef SMG_WITH_BLOOM
#    include "bloomrenderer.hh"
#endif

namespace smg {

ScenePanel::ScenePanel() = default;

ShScenePanelPr ScenePanel::create() { return std::make_shared<ScenePanel>(); }

std::size_t ScenePanel::add(ShMeshPr mesh, const Magnum::Matrix4& transform, const Magnum::Color3& color) {
    _objects.push_back(Object{ std::move(mesh), transform, color, false, true });
    _fitted = false; // refit when the scene changes
    return _objects.size() - 1;
}

std::size_t ScenePanel::add_cube(const Magnum::Matrix4& t, const Magnum::Color3& c) { return add(primitives::cube(), t, c); }
std::size_t ScenePanel::add_sphere(const Magnum::Matrix4& t, const Magnum::Color3& c) { return add(primitives::sphere(), t, c); }
std::size_t ScenePanel::add_plane(const Magnum::Matrix4& t, const Magnum::Color3& c) { return add(primitives::plane(), t, c); }
std::size_t ScenePanel::add_grid(const Magnum::Matrix4& t, const Magnum::Color3& c) {
    const std::size_t h = add(primitives::grid(Magnum::Vector2i{ 10, 10 }, c), t, c);
    _objects[h].wireframe = true;
    return h;
}
std::size_t ScenePanel::add_axes(float scale, const Magnum::Matrix4& t) {
    const std::size_t h = add(primitives::axes(scale), t, Magnum::Color3{ 1.0f });
    _objects[h].wireframe = true; // unlit, vertex-coloured lines
    return h;
}

std::size_t ScenePanel::add_sprite(ShSpriteSheetPr sheet, int frame, const Magnum::Vector3& position, const SpriteParams& params) {
    _sprites.push_back(Sprite{ std::move(sheet), frame, position, params });
    return _sprites.size() - 1;
}

std::optional<Magnum::Vector3> ScenePanel::cursor_world() const {
    if(_fbo_size.x() <= 0 || _fbo_size.y() <= 0) return {};
    const ImVec2 m = ImGui::GetMousePos();
    const Magnum::Vector2 local{ m.x - _image_min.x(), m.y - _image_min.y() };
    if(local.x() < 0.0f || local.y() < 0.0f || local.x() > float(_fbo_size.x()) || local.y() > float(_fbo_size.y())) return {};
    const Camera::Ray ray = _camera.unproject(local, Magnum::Vector2{ _fbo_size });
    return ray_ground(ray, _camera.up_axis());
}

std::optional<Magnum::Vector2i> ScenePanel::tile_under_cursor(const IsoGrid& grid) const {
    const std::optional<Magnum::Vector3> w = cursor_world();
    if(!w) return {};
    return grid.to_cell(*w);
}

void ScenePanel::clear() {
    _objects.clear();
    _sprites.clear();
    _fitted = false;
}

void ScenePanel::fit() {
    Bounds scene;
    for(const Object& o : _objects) {
        if(!o.visible || !o.mesh) continue;
        scene.expand(transformed(o.mesh->bounds(), o.transform));
    }
    for(const Sprite& s : _sprites) scene.expand(s.position);
    _camera.fit(scene);
    _fitted = true;
}

void ScenePanel::ensure_gl() {
    if(_gl_ready) return;
    _phong = Magnum::Shaders::PhongGL{ Magnum::Shaders::PhongGL::Configuration{}.setFlags(Magnum::Shaders::PhongGL::Flag::VertexColor) };
    _vcolor = Magnum::Shaders::VertexColorGL3D{};
    _sprite_renderer = std::make_shared<SpriteRenderer>();
    _gl_ready = true;
}

void ScenePanel::ensure_fbo(const Magnum::Vector2i& size) {
    if(_fbo_size == size && _color.id() != 0) return;
    _fbo_size = size;
    _color = Magnum::GL::Texture2D{};
#ifdef CORRADE_TARGET_EMSCRIPTEN
    // WebGL2: RGBA8 texture format and multisampled renderbuffers are not available;
    // use RGBA and disable MSAA.
    _use_msaa = false;
    _color.setStorage(1, Magnum::GL::TextureFormat::RGBA, size)
        .setMinificationFilter(Magnum::GL::SamplerFilter::Linear)
        .setMagnificationFilter(Magnum::GL::SamplerFilter::Linear);
    _depth = Magnum::GL::Renderbuffer{};
    _depth.setStorage(Magnum::GL::RenderbufferFormat::DepthStencil, size);
#else
    _color.setStorage(1, Magnum::GL::TextureFormat::RGBA8, size)
        .setMinificationFilter(Magnum::GL::SamplerFilter::Linear)
        .setMagnificationFilter(Magnum::GL::SamplerFilter::Linear);
    _depth = Magnum::GL::Renderbuffer{};
    _depth.setStorage(Magnum::GL::RenderbufferFormat::Depth24Stencil8, size);
#endif
    _fbo = Magnum::GL::Framebuffer{ Magnum::Range2Di::fromSize({}, size) };
    _fbo.attachTexture(Magnum::GL::Framebuffer::ColorAttachment{ 0 }, _color, 0)
        .attachRenderbuffer(Magnum::GL::Framebuffer::BufferAttachment::DepthStencil, _depth);
    CORRADE_INTERNAL_ASSERT(_fbo.checkStatus(Magnum::GL::FramebufferTarget::Draw) == Magnum::GL::Framebuffer::Status::Complete);
#ifndef CORRADE_TARGET_EMSCRIPTEN
    if(_use_msaa) {
        _colorMsaa = Magnum::GL::Renderbuffer{};
        _colorMsaa.setStorageMultisample(_samples, Magnum::GL::RenderbufferFormat::RGBA8, size);
        _depthMsaa = Magnum::GL::Renderbuffer{};
        _depthMsaa.setStorageMultisample(_samples, Magnum::GL::RenderbufferFormat::Depth24Stencil8, size);
        _msaaFbo = Magnum::GL::Framebuffer{ Magnum::Range2Di::fromSize({}, size) };
        _msaaFbo.attachRenderbuffer(Magnum::GL::Framebuffer::ColorAttachment{ 0 }, _colorMsaa)
            .attachRenderbuffer(Magnum::GL::Framebuffer::BufferAttachment::DepthStencil, _depthMsaa);
        CORRADE_INTERNAL_ASSERT(_msaaFbo.checkStatus(Magnum::GL::FramebufferTarget::Draw) == Magnum::GL::Framebuffer::Status::Complete);
    }
#endif

#ifdef SMG_WITH_BLOOM
    // color-only target for the post-bloom composite (no depth needed)
    _postColor = Magnum::GL::Texture2D{};
    _postColor.setStorage(1, Magnum::GL::TextureFormat::RGBA8, size)
        .setMinificationFilter(Magnum::GL::SamplerFilter::Linear)
        .setMagnificationFilter(Magnum::GL::SamplerFilter::Linear);
    _postFbo = Magnum::GL::Framebuffer{ Magnum::Range2Di::fromSize({}, size) };
    _postFbo.attachTexture(Magnum::GL::Framebuffer::ColorAttachment{ 0 }, _postColor, 0);
    CORRADE_INTERNAL_ASSERT(_postFbo.checkStatus(Magnum::GL::FramebufferTarget::Draw) == Magnum::GL::Framebuffer::Status::Complete);
    if(!_bloom) _bloom = bloom::BloomRenderer::create();
    _bloom->initialize(size, true);
#endif
}

void ScenePanel::render_scene(const Magnum::Vector2i& size) {
    const float aspect = size.y() > 0 ? float(size.x()) / float(size.y()) : 1.0f;
    const Magnum::Matrix4 view = _camera.view();
    const Magnum::Matrix4 proj = _camera.projection(aspect);

    Magnum::GL::Framebuffer& target = _use_msaa ? _msaaFbo : _fbo;
    target.bind();
#ifdef CORRADE_TARGET_EMSCRIPTEN
    // WebGL1 (GLES2): clearColor/clearDepth on Framebuffer not available; use Renderer
    Magnum::GL::Renderer::setClearColor(Magnum::Color4{ 0.12f, 0.12f, 0.14f, 1.0f });
    Magnum::GL::Renderer::setClearDepth(1.0f);
    target.clear(Magnum::GL::FramebufferClear::Color | Magnum::GL::FramebufferClear::Depth);
#else
    target.clearColor(0, Magnum::Color4{ 0.12f, 0.12f, 0.14f, 1.0f }).clearDepth(1.0f);
#endif

    Magnum::GL::Renderer::enable(Magnum::GL::Renderer::Feature::DepthTest);
    Magnum::GL::Renderer::enable(Magnum::GL::Renderer::Feature::FaceCulling);

    for(Object& o : _objects) {
        if(!o.visible || !o.mesh) continue;
        const Magnum::Matrix4 tv = view * o.transform;
        if(o.wireframe) {
            // unlit, vertex-coloured lines/wireframe
            _vcolor.setTransformationProjectionMatrix(proj * tv).draw(o.mesh->gl());
        } else {
            _phong.setTransformationMatrix(tv)
                .setNormalMatrix(tv.normalMatrix())
                .setProjectionMatrix(proj)
                .setLightPositions({ Magnum::Vector4{ _light.position, 0.0f } })
                .setAmbientColor(_light.ambient)
                .setDiffuseColor(o.color)
                .setSpecularColor(_light.specular)
                .setShininess(_light.shininess)
                .draw(o.mesh->gl());
        }
    }

    // sprites share the depth buffer; quads are double-sided, so drop face culling
    Magnum::GL::Renderer::disable(Magnum::GL::Renderer::Feature::FaceCulling);
    if(_sprite_renderer && !_sprites.empty()) _sprite_renderer->draw(_sprites, view, proj);

    Magnum::GL::Renderer::disable(Magnum::GL::Renderer::Feature::DepthTest);
#ifndef CORRADE_TARGET_EMSCRIPTEN
    if(_use_msaa) {
        Magnum::GL::Framebuffer::blit(_msaaFbo,
            _fbo,
            Magnum::Range2Di::fromSize({}, size),
            Magnum::Range2Di::fromSize({}, size),
            Magnum::GL::FramebufferBlit::Color,
            Magnum::GL::FramebufferBlitFilter::Nearest);
    }
#endif
    Magnum::GL::defaultFramebuffer.bind();
}

void ScenePanel::handle_input(const Magnum::Vector2& image_size) {
    (void)image_size;
    if(!ImGui::IsItemHovered()) return;
    const ImGuiIO& io = ImGui::GetIO();
    const float dx = io.MouseDelta.x;
    const float dy = io.MouseDelta.y;
    if(ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        _camera.orbit(dx, dy);
    else if(ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
        _camera.pan(dx, dy);
    else if(ImGui::IsMouseDragging(ImGuiMouseButton_Right))
        _camera.zoom(-dy * 0.1f);
    if(io.MouseWheel != 0.0f) _camera.zoom(io.MouseWheel);
}

// clamped to the slider ranges in draw() so UI and API cannot diverge; a
// consumer over the ABI boundary gets degradation rather than an abort
void ScenePanel::set_bloom_strength(float strength) { _bloom_strength = std::clamp(strength, 0.0f, 1.0f); }

void ScenePanel::set_bloom_radius(float radius) { _bloom_radius = std::clamp(radius, 0.001f, 0.02f); }

#ifdef SMG_WITH_BLOOM
Magnum::GL::Texture2D& ScenePanel::bloom_pass(const Magnum::Vector2i& size) {
    (void)size;
    _bloom->set_bloom_strength(_bloom_strength);
    _bloom->set_filter_radius(_bloom_radius);
    _bloom->render_bloom_texture(_color);

    // render_final composites into the currently bound FBO; _postFbo.bind()
    // restores its construction viewport, so no manual viewport fix is needed.
    _postFbo.bind();
    _postFbo.clearColor(0, Magnum::Color4{ 0.0f });
    _bloom->render_final(_color);
    Magnum::GL::defaultFramebuffer.bind();
    return _postColor;
}
#endif

void ScenePanel::draw(const char* title, const Magnum::Vector2i& size) {
    ensure_gl();
    ensure_fbo(size);
    if(!_fitted) fit();
    render_scene(size);

    Magnum::GL::Texture2D* shown = &_color;
#ifdef SMG_WITH_BLOOM
    if(_bloom_enabled) shown = &bloom_pass(size);
#endif

    ImGui::Begin(title);
    Magnum::ImGuiIntegration::image(*shown, Magnum::Vector2{ size });
    const ImVec2 imin = ImGui::GetItemRectMin();
    _image_min = Magnum::Vector2{ imin.x, imin.y };
    handle_input(Magnum::Vector2{ size });
#ifdef SMG_WITH_BLOOM
    ImGui::Checkbox("Bloom", &_bloom_enabled);
    if(_bloom_enabled) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        ImGui::SliderFloat("strength", &_bloom_strength, 0.0f, 1.0f);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        ImGui::SliderFloat("radius", &_bloom_radius, 0.001f, 0.02f, "%.4f");
    }
#endif
    ImGui::End();
}

} // namespace smg
