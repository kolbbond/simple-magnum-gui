// primitive mesh generators (cube/sphere/plane/grid/axes)
#pragma once

#include <vector>

#include <Magnum/Math/Color.h>
#include <Magnum/Math/Vector2.h>

#include "Mesh.hh"
#include "SceneTypes.hh"

namespace smg::primitives {

// CPU-side axis gizmo geometry (headless-testable)
[[nodiscard]] std::vector<Vertex> axes_vertices(float scale);

// CPU-side XZ ground-plane wireframe grid geometry (headless-testable)
[[nodiscard]] std::vector<Vertex> grid_vertices(const Magnum::Vector2i& subdivisions, const Magnum::Color3& color);

[[nodiscard]] ShMeshPr cube();
[[nodiscard]] ShMeshPr sphere(unsigned subdivisions = 2);
[[nodiscard]] ShMeshPr plane();
[[nodiscard]] ShMeshPr grid(const Magnum::Vector2i& subdivisions = Magnum::Vector2i{ 10, 10 },
    const Magnum::Color3& color = Magnum::Color3{ 0.4f });
[[nodiscard]] ShMeshPr axes(float scale = 1.0f);

} // namespace smg::primitives
