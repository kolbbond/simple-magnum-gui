// scene value types: vertex, AABB, light properties
#pragma once

#include <Corrade/Containers/ArrayView.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Vector3.h>

namespace smg {

// interleaved vertex for lit, colored meshes
struct Vertex {
    Magnum::Vector3 position;
    Magnum::Vector3 normal{ 0.0f, 0.0f, 1.0f };
    Magnum::Color3 color{ 1.0f, 1.0f, 1.0f };
};

// axis-aligned bounding box; default-constructed is empty (min > max)
struct Bounds {
    Magnum::Vector3 min;
    Magnum::Vector3 max;

    Bounds();
    [[nodiscard]] bool empty() const;
    void expand(const Magnum::Vector3& p);
    void expand(const Bounds& b);
    [[nodiscard]] Magnum::Vector3 center() const;
    [[nodiscard]] Magnum::Vector3 size() const;
    [[nodiscard]] float diagonal() const;
};

// compute an AABB over a vertex span
[[nodiscard]] Bounds compute_bounds(Corrade::Containers::ArrayView<const Vertex> verts);

// single Phong light + material defaults
struct LightProperties {
    Magnum::Color3 ambient{ 0.6f, 0.6f, 0.6f };
    Magnum::Color3 diffuse{ 1.0f, 1.0f, 1.0f };
    Magnum::Color3 specular{ 0.9f, 0.9f, 0.9f };
    Magnum::Vector3 position{ 5.0f, 5.0f, 5.0f };
    float shininess{ 64.0f };
};

} // namespace smg
