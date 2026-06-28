#include "SpriteRenderer.hh"

#include <algorithm>

#include <Corrade/Containers/Array.h>
#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/Mesh.h>
#include <Magnum/Math/Matrix3.h>
#include <Magnum/Math/Vector4.h>
#include <Magnum/Shaders/GenericGL.h>

#include "SpriteSheet.hh"

namespace smg {

namespace {
struct QuadVertex {
    Magnum::Vector3 position;
    Magnum::Vector2 uv;
};
} // namespace

SpriteRenderer::SpriteRenderer() {
    // unit quad in the local XY plane; UV origin top-left (v grows downward)
    const QuadVertex verts[]{
        { { -0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f } }, // bottom-left
        { { 0.5f, -0.5f, 0.0f }, { 1.0f, 1.0f } }, // bottom-right
        { { 0.5f, 0.5f, 0.0f }, { 1.0f, 0.0f } }, // top-right
        { { -0.5f, 0.5f, 0.0f }, { 0.0f, 0.0f } }, // top-left
    };
    const Magnum::UnsignedInt indices[]{ 0, 1, 2, 0, 2, 3 };

    Magnum::GL::Buffer vb;
    vb.setData(verts);
    Magnum::GL::Buffer ib;
    ib.setData(indices);

    _quad = Magnum::GL::Mesh{};
    _quad.setPrimitive(Magnum::MeshPrimitive::Triangles)
        .addVertexBuffer(std::move(vb), 0, Magnum::Shaders::GenericGL3D::Position{}, Magnum::Shaders::GenericGL3D::TextureCoordinates{})
        .setCount(6)
        .setIndexBuffer(std::move(ib), 0, Magnum::MeshIndexType::UnsignedInt);

    _alphaMask = Magnum::Shaders::FlatGL3D{ Magnum::Shaders::FlatGL3D::Configuration{}.setFlags(Magnum::Shaders::FlatGL3D::Flag::Textured |
        Magnum::Shaders::FlatGL3D::Flag::TextureTransformation | Magnum::Shaders::FlatGL3D::Flag::AlphaMask) };
    _blended = Magnum::Shaders::FlatGL3D{ Magnum::Shaders::FlatGL3D::Configuration{}.setFlags(
        Magnum::Shaders::FlatGL3D::Flag::Textured | Magnum::Shaders::FlatGL3D::Flag::TextureTransformation) };
}

void SpriteRenderer::draw_one(Magnum::Shaders::FlatGL3D& shader,
    const Sprite& s,
    const Magnum::Matrix4& view,
    const Magnum::Matrix4& projection,
    const Magnum::Vector3& right,
    const Magnum::Vector3& up,
    const Magnum::Vector3& fwd) {
    if(!s.sheet) return;
    const Magnum::Vector2 off = anchor_offset(s.params.anchor);
    const Magnum::Vector3 center = s.position + right * (off.x() * s.params.size.x()) + up * (off.y() * s.params.size.y());
    const Magnum::Matrix3 basis{ right * s.params.size.x(), up * s.params.size.y(), fwd };
    const Magnum::Matrix4 model = Magnum::Matrix4::from(basis, center);

    const Magnum::Range2D uv = s.sheet->frame_uv(s.frame);
    const Magnum::Matrix3 texMat = Magnum::Matrix3::translation(uv.min()) * Magnum::Matrix3::scaling(uv.size());

    shader.setTransformationProjectionMatrix(projection * view * model)
        .setTextureMatrix(texMat)
        .setColor(s.params.tint)
        .bindTexture(s.sheet->texture());
    shader.draw(_quad);
}

void SpriteRenderer::draw(const std::vector<Sprite>& sprites, const Magnum::Matrix4& view, const Magnum::Matrix4& projection) {
    const Magnum::Matrix4 camWorld = view.invertedRigid();
    const Magnum::Vector3 right = camWorld[0].xyz().normalized();
    const Magnum::Vector3 up = camWorld[1].xyz().normalized();
    const Magnum::Vector3 fwd = camWorld[2].xyz().normalized();

    // pass 1: alpha-masked sprites, depth write on, no sorting needed
    _alphaMask.setAlphaMask(0.5f);
    for(const Sprite& s : sprites) {
        if(!s.params.visible || s.params.blend != SpriteBlend::AlphaMask) continue;
        draw_one(_alphaMask, s, view, projection, right, up, fwd);
    }

    // pass 2: blended/additive sprites, back-to-front, depth test on but no depth write
    std::vector<const Sprite*> blended;
    for(const Sprite& s : sprites) {
        if(s.params.visible && s.params.blend != SpriteBlend::AlphaMask) blended.push_back(&s);
    }
    if(blended.empty()) return;

    std::sort(blended.begin(), blended.end(), [&view](const Sprite* a, const Sprite* b) {
        // view-space z: more negative = farther; draw farthest first
        const float za = (view * Magnum::Vector4{ a->position, 1.0f }).z();
        const float zb = (view * Magnum::Vector4{ b->position, 1.0f }).z();
        return za < zb;
    });

    Magnum::GL::Renderer::enable(Magnum::GL::Renderer::Feature::Blending);
    Magnum::GL::Renderer::setDepthMask(false);
    for(const Sprite* s : blended) {
        if(s->params.blend == SpriteBlend::Additive)
            Magnum::GL::Renderer::setBlendFunction(
                Magnum::GL::Renderer::BlendFunction::SourceAlpha, Magnum::GL::Renderer::BlendFunction::One);
        else
            Magnum::GL::Renderer::setBlendFunction(
                Magnum::GL::Renderer::BlendFunction::SourceAlpha, Magnum::GL::Renderer::BlendFunction::OneMinusSourceAlpha);
        draw_one(_blended, *s, view, projection, right, up, fwd);
    }
    Magnum::GL::Renderer::setDepthMask(true);
    Magnum::GL::Renderer::disable(Magnum::GL::Renderer::Feature::Blending);
}

} // namespace smg
