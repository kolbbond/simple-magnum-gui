#include "ScenePanel.hh"

#include <algorithm>
#include <array>

#include <Corrade/Utility/Assert.h>
#include <Magnum/GL/RenderbufferFormat.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/ImGuiIntegration/Integration.h>
#include <Magnum/ImGuiIntegration/Widgets.h>

#include <imgui.h>

#include "GlStateGuard.hh"
#include "IsoGrid.hh"
#include "Primitives.hh"
#include "SectionShaders.hh"
#include "SpriteRenderer.hh"

#ifdef SMG_WITH_BLOOM
#    include "bloomrenderer.hh"
#endif

namespace smg {

Magnum::Vector4 SectionPlane::equation() const {
    const float len = normal.length();
    if(!enabled || len <= 0.0f) return Magnum::Vector4{ 0.0f, 0.0f, 0.0f, -1.0f };
    return Magnum::Vector4{ normal / len, -offset };
}

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

Bounds ScenePanel::scene_bounds() const {
    Bounds scene;
    for(const Object& o : _objects) {
        if(!o.visible || !o.mesh) continue;
        scene.expand(transformed(o.mesh->bounds(), o.transform));
    }
    for(const Sprite& s : _sprites) {
        if(!s.params.visible) continue;
        // billboards turn with the camera; |size| bounds the quad for any anchor and view
        const Magnum::Vector3 r{ s.params.size.length() };
        scene.expand(s.position - r);
        scene.expand(s.position + r);
    }
    return scene;
}

void ScenePanel::fit() {
    _camera.fit(scene_bounds());
    _fitted = true;
}

void ScenePanel::ensure_gl() {
    if(_gl_ready) return;
    _phong = std::make_shared<SectionPhongGL>();
    _vcolor = std::make_shared<SectionVertexColorGL>();
    _sprite_renderer = std::make_shared<SpriteRenderer>();
    _gl_ready = true;
}

void ScenePanel::ensure_fbo(const Magnum::Vector2i& size) {
    if(_fbo_size == size && _color.id() != 0) return;
    _fbo_size = size;
    _color = Magnum::GL::Texture2D{};
    _color.setStorage(1, Magnum::GL::TextureFormat::RGBA8, size)
        .setMinificationFilter(Magnum::GL::SamplerFilter::Linear)
        .setMagnificationFilter(Magnum::GL::SamplerFilter::Linear);
    _depth = Magnum::GL::Renderbuffer{};
    _depth.setStorage(Magnum::GL::RenderbufferFormat::Depth24Stencil8, size);
    _fbo = Magnum::GL::Framebuffer{ Magnum::Range2Di::fromSize({}, size) };
    _fbo.attachTexture(Magnum::GL::Framebuffer::ColorAttachment{ 0 }, _color, 0)
        .attachRenderbuffer(Magnum::GL::Framebuffer::BufferAttachment::DepthStencil, _depth);
    CORRADE_INTERNAL_ASSERT(_fbo.checkStatus(Magnum::GL::FramebufferTarget::Draw) == Magnum::GL::Framebuffer::Status::Complete);
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
    target.clearColor(0, Magnum::Color4{ 0.12f, 0.12f, 0.14f, 1.0f }).clearDepth(1.0f);

    Magnum::GL::Renderer::enable(Magnum::GL::Renderer::Feature::DepthTest);
    // a cut solid shows its back faces through the opening; they draw as the cap
    if(_section.enabled)
        Magnum::GL::Renderer::disable(Magnum::GL::Renderer::Feature::FaceCulling);
    else
        Magnum::GL::Renderer::enable(Magnum::GL::Renderer::Feature::FaceCulling);

    // plane equations transform by the inverse transpose
    const Magnum::Vector4 plane = view.inverted().transposed() * _section.equation();
    _phong->set_projection(proj)
        .set_light_direction(_light.position)
        .set_specular(_light.specular)
        .set_shininess(_light.shininess)
        .set_section(plane)
        .set_cap_color(_section.cap_color);
    _vcolor->set_projection(proj).set_section(plane);

    for(Object& o : _objects) {
        if(!o.visible || !o.mesh) continue;
        const Magnum::Matrix4 tv = view * o.transform;
        if(o.wireframe) {
            // unlit, vertex-coloured lines/wireframe
            _vcolor->set_transformation(tv).draw(o.mesh->gl());
        } else {
            // like Magnum's Phong, the shader leaves material colour to the caller for both terms
            _phong->set_transformation(tv)
                .set_normal_matrix(tv.normalMatrix())
                .set_ambient(_light.ambient * o.color)
                .set_diffuse(_light.diffuse * o.color)
                .draw(o.mesh->gl());
        }
    }

    // sprites share the depth buffer; quads are double-sided, so drop face culling
    Magnum::GL::Renderer::disable(Magnum::GL::Renderer::Feature::FaceCulling);
    if(_sprite_renderer && !_sprites.empty()) _sprite_renderer->draw(_sprites, view, proj, _section.equation());

    Magnum::GL::Renderer::disable(Magnum::GL::Renderer::Feature::DepthTest);
    if(_use_msaa) {
        Magnum::GL::Framebuffer::blit(_msaaFbo,
            _fbo,
            Magnum::Range2Di::fromSize({}, size),
            Magnum::Range2Di::fromSize({}, size),
            Magnum::GL::FramebufferBlit::Color,
            Magnum::GL::FramebufferBlitFilter::Nearest);
    }
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

void ScenePanel::draw_section_controls() {
    ImGui::Checkbox("Section", &_section.enabled);
    if(!_section.enabled) return;

    // UI edits axis-aligned planes; an arbitrary API-set normal shows as its dominant axis
    const Magnum::Vector3 a = Magnum::Math::abs(_section.normal);
    int axis = a.x() >= a.y() && a.x() >= a.z() ? 0 : (a.y() >= a.z() ? 1 : 2);
    bool flip = _section.normal[axis] < 0.0f;
    bool changed = false;
    const std::array<const char*, 3> labels{ "X", "Y", "Z" };
    for(int i = 0; i < int(labels.size()); ++i) {
        ImGui::SameLine();
        changed |= ImGui::RadioButton(labels[std::size_t(i)], &axis, i);
    }
    ImGui::SameLine();
    changed |= ImGui::Checkbox("flip", &flip);
    if(changed) {
        _section.normal = Magnum::Vector3{};
        _section.normal[axis] = flip ? -1.0f : 1.0f;
    }

    const Bounds b = scene_bounds();
    float lo = -1.0f;
    float hi = 1.0f;
    if(!b.empty()) {
        const Magnum::Vector3 n = _section.normal.normalized();
        // extent of the AABB along n: center projection +- half-size projected on |n|
        const float c = Magnum::Math::dot(n, b.center());
        const float r = Magnum::Math::dot(Magnum::Math::abs(n), b.size() * 0.5f);
        lo = c - r;
        hi = c + r;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f);
    ImGui::SliderFloat("offset", &_section.offset, lo, hi);
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
    return _postColor;
}
#endif

void ScenePanel::draw(const char* title, const Magnum::Vector2i& size) {
    // a collapsed or minimised host reports 0; a zero-size FBO is incomplete and asserts
    if(size.x() <= 0 || size.y() <= 0) {
        ImGui::Begin(title);
        ImGui::End();
        return;
    }
    Magnum::GL::Texture2D* shown = &_color;
    {
        const GlStateGuard restore;
        ensure_gl();
        ensure_fbo(size);
        if(!_fitted) fit();
        // objects may be added/moved after fit; keep auto near/far tracking them
        _camera.set_scene_bounds(scene_bounds());
        render_scene(size);
#ifdef SMG_WITH_BLOOM
        if(_bloom_enabled) shown = &bloom_pass(size);
#endif
    }

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
    draw_section_controls();
    ImGui::End();
}

std::size_t scene_panel_abi_size() { return sizeof(ScenePanel); }

} // namespace smg
