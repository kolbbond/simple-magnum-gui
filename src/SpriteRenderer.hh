// internal GL sprite pass: textured camera-facing billboards (alpha-mask + blended)
#pragma once

#include <vector>

#include <Magnum/GL/Mesh.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/Shaders/FlatGL.h>

#include "SpriteTypes.hh"

namespace smg {

class SpriteRenderer {
public:
    SpriteRenderer();

    // draw all visible sprites; expects DepthTest already enabled by the caller
    void draw(const std::vector<Sprite>& sprites, const Magnum::Matrix4& view, const Magnum::Matrix4& projection);

private:
    void draw_one(Magnum::Shaders::FlatGL3D& shader,
        const Sprite& s,
        const Magnum::Matrix4& view,
        const Magnum::Matrix4& projection,
        const Magnum::Vector3& right,
        const Magnum::Vector3& up,
        const Magnum::Vector3& fwd);

    Magnum::GL::Mesh _quad{ Magnum::NoCreate };
    Magnum::Shaders::FlatGL3D _alphaMask{ Magnum::NoCreate };
    Magnum::Shaders::FlatGL3D _blended{ Magnum::NoCreate };
};

} // namespace smg
