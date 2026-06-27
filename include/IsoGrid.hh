// isometric tile <-> world mapping on the ground plane (headless)
#pragma once

#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector3.h>

#include "Camera.hh" // UpAxis

namespace smg {

struct IsoGrid {
    float tile_size{ 1.0f };
    UpAxis up{ UpAxis::Y };

    [[nodiscard]] Magnum::Vector3 to_world(const Magnum::Vector2i& cell) const;
    [[nodiscard]] Magnum::Vector2i to_cell(const Magnum::Vector3& world) const;
};

} // namespace smg
