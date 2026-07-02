// embeddable 3D scene viewport: renders a retained scene into an ImGui window
#pragma once

#include <cassert>
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include <Magnum/GL/Framebuffer.h>
#include <Magnum/GL/Renderbuffer.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/Shaders/PhongGL.h>
#include <Magnum/Shaders/VertexColorGL.h>

#include "Camera.hh"
#include "IsoGrid.hh"
#include "Mesh.hh"
#include "SceneTypes.hh"
#include "SpriteSheet.hh"
#include "SpriteTypes.hh"

#ifdef SMG_WITH_BLOOM
namespace bloom {
class BloomRenderer;
}
#endif

namespace smg {

class SpriteRenderer;

typedef std::shared_ptr<class ScenePanel> ShScenePanelPr;

class ScenePanel {
public:
    struct Object {
        ShMeshPr mesh;
        Magnum::Matrix4 transform;
        Magnum::Color3 color{ 1.0f };
        bool wireframe{ false };
        bool visible{ true };
    };

    ScenePanel();
    static ShScenePanelPr create();

    std::size_t add(ShMeshPr mesh, const Magnum::Matrix4& transform = {}, const Magnum::Color3& color = Magnum::Color3{ 1.0f });
    std::size_t add_cube(const Magnum::Matrix4& transform = {}, const Magnum::Color3& color = Magnum::Color3{ 1.0f });
    std::size_t add_sphere(const Magnum::Matrix4& transform = {}, const Magnum::Color3& color = Magnum::Color3{ 1.0f });
    std::size_t add_plane(const Magnum::Matrix4& transform = {}, const Magnum::Color3& color = Magnum::Color3{ 1.0f });
    std::size_t add_grid(const Magnum::Matrix4& transform = {}, const Magnum::Color3& color = Magnum::Color3{ 0.4f });
    std::size_t add_axes(float scale = 1.0f, const Magnum::Matrix4& transform = {});

    std::size_t add_sprite(ShSpriteSheetPr sheet, int frame, const Magnum::Vector3& position, const SpriteParams& params = {});
    // handles are indices; add_sprite()/clear() invalidate previously returned references
    [[nodiscard]] Sprite& sprite(std::size_t handle) {
        assert(handle < _sprites.size());
        return _sprites[handle];
    }
    [[nodiscard]] const Sprite& sprite(std::size_t handle) const {
        assert(handle < _sprites.size());
        return _sprites[handle];
    }

    // ground-plane point / iso-grid cell under the mouse (valid only after draw())
    [[nodiscard]] std::optional<Magnum::Vector3> cursor_world() const;
    [[nodiscard]] std::optional<Magnum::Vector2i> tile_under_cursor(const IsoGrid& grid) const;

    void clear();
    // handles are indices; add*()/clear() invalidate previously returned references
    [[nodiscard]] Object& object(std::size_t handle) {
        assert(handle < _objects.size());
        return _objects[handle];
    }
    [[nodiscard]] const Object& object(std::size_t handle) const {
        assert(handle < _objects.size());
        return _objects[handle];
    }
    [[nodiscard]] std::size_t object_count() const { return _objects.size(); }
    [[nodiscard]] Camera& camera() { return _camera; }
    [[nodiscard]] const Camera& camera() const { return _camera; }
    [[nodiscard]] LightProperties& light() { return _light; }
    [[nodiscard]] const LightProperties& light() const { return _light; }
    void fit();

    void draw(const char* title, const Magnum::Vector2i& size = Magnum::Vector2i{ 640, 480 });

protected:
    void ensure_gl();
    void ensure_fbo(const Magnum::Vector2i& size);
    void render_scene(const Magnum::Vector2i& size);

    std::vector<Object> _objects;
    Camera _camera;
    LightProperties _light;
    bool _fitted{ false };

    std::vector<Sprite> _sprites;
    std::shared_ptr<SpriteRenderer> _sprite_renderer;
    mutable Magnum::Vector2 _image_min{ 0.0f, 0.0f }; // last drawn image top-left, screen px

    void handle_input(const Magnum::Vector2& image_size);

    bool _gl_ready{ false };
    Magnum::Vector2i _fbo_size{ 0, 0 };
    Magnum::Shaders::PhongGL _phong{ Magnum::NoCreate };
    Magnum::Shaders::VertexColorGL3D _vcolor{ Magnum::NoCreate };
    Magnum::GL::Texture2D _color{ Magnum::NoCreate };
    Magnum::GL::Renderbuffer _depth{ Magnum::NoCreate };
    Magnum::GL::Framebuffer _fbo{ Magnum::NoCreate };

    bool _use_msaa{ true };
    int _samples{ 4 };
    Magnum::GL::Renderbuffer _colorMsaa{ Magnum::NoCreate };
    Magnum::GL::Renderbuffer _depthMsaa{ Magnum::NoCreate };
    Magnum::GL::Framebuffer _msaaFbo{ Magnum::NoCreate };

#ifdef SMG_WITH_BLOOM
    // optional glow pass (desktop only): scene -> bloom -> _postFbo -> ImGui
    Magnum::GL::Texture2D& bloom_pass(const Magnum::Vector2i& size);
    std::shared_ptr<bloom::BloomRenderer> _bloom;
    Magnum::GL::Texture2D _postColor{ Magnum::NoCreate };
    Magnum::GL::Framebuffer _postFbo{ Magnum::NoCreate };
    bool _bloom_enabled{ true };
    float _bloom_strength{ 0.3f };
    float _bloom_radius{ 0.005f };
#endif
};

} // namespace smg
