#include "ScenePanel.hh"

#include <Corrade/Utility/Assert.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/RenderbufferFormat.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/ImGuiIntegration/Integration.h>
#include <Magnum/ImGuiIntegration/Widgets.h>

#include <imgui.h>

#include "Primitives.hh"

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
    const std::size_t h = add(primitives::grid(), t, c);
    _objects[h].wireframe = true;
    return h;
}
std::size_t ScenePanel::add_axes(float scale, const Magnum::Matrix4& t) {
    const std::size_t h = add(primitives::axes(scale), t, Magnum::Color3{ 1.0f });
    _objects[h].wireframe = true; // unlit, vertex-coloured lines
    return h;
}

void ScenePanel::clear() {
    _objects.clear();
    _fitted = false;
}

void ScenePanel::fit() {
    Bounds scene;
    for(const Object& o : _objects)
        if(o.mesh) scene.expand(o.mesh->bounds());
    _camera.fit(scene);
    _fitted = true;
}

void ScenePanel::ensure_gl() {
    if(_gl_ready) return;
    _phong = Magnum::Shaders::PhongGL{ Magnum::Shaders::PhongGL::Configuration{}.setFlags(Magnum::Shaders::PhongGL::Flag::VertexColor) };
    _vcolor = Magnum::Shaders::VertexColorGL3D{};
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
}

void ScenePanel::render_scene(const Magnum::Vector2i& size) {
    const float aspect = size.y() > 0 ? float(size.x()) / float(size.y()) : 1.0f;
    const Magnum::Matrix4 view = _camera.view();
    const Magnum::Matrix4 proj = _camera.projection(aspect);

    _fbo.clearColor(0, Magnum::Color4{ 0.12f, 0.12f, 0.14f, 1.0f }).clearDepth(1.0f).bind();

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

    Magnum::GL::Renderer::disable(Magnum::GL::Renderer::Feature::DepthTest);
    Magnum::GL::Renderer::disable(Magnum::GL::Renderer::Feature::FaceCulling);
    Magnum::GL::defaultFramebuffer.bind();
}

void ScenePanel::draw(const char* title, const Magnum::Vector2i& size) {
    ensure_gl();
    ensure_fbo(size);
    if(!_fitted) fit();
    render_scene(size);

    ImGui::Begin(title);
    Magnum::ImGuiIntegration::image(_color, Magnum::Vector2{ size });
    ImGui::End();
}

} // namespace smg
