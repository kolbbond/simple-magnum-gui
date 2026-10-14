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

    // draw all visible sprites; expects DepthTest already enabled by the caller.
    // sprites whose position is on the positive side of the world-space clip plane are skipped
    void draw(const std::vector<Sprite>& sprites,
        const Magnum::Matrix4& view,
        const Magnum::Matrix4& projection,
        const Magnum::Vector4& clip_plane = Magnum::Vector4{ 0.0f, 0.0f, 0.0f, -1.0f });

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
