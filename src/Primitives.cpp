#include "Primitives.hh"

#include <Magnum/GL/Mesh.h>
#include <Magnum/Primitives/Cube.h>
#include <Magnum/Primitives/Grid.h>
#include <Magnum/Primitives/Icosphere.h>
#include <Magnum/Primitives/Plane.h>
#include <Magnum/Trade/MeshData.h>

namespace smg::primitives {

std::vector<Vertex> axes_vertices(float scale) {
    const Magnum::Color3 red{ 1.0f, 0.0f, 0.0f };
    const Magnum::Color3 green{ 0.0f, 1.0f, 0.0f };
    const Magnum::Color3 blue{ 0.0f, 0.0f, 1.0f };
    const Magnum::Vector3 origin{ 0.0f, 0.0f, 0.0f };
    return { Vertex{ origin, {}, red },
        Vertex{ Magnum::Vector3{ scale, 0.0f, 0.0f }, {}, red },
        Vertex{ origin, {}, green },
        Vertex{ Magnum::Vector3{ 0.0f, scale, 0.0f }, {}, green },
        Vertex{ origin, {}, blue },
        Vertex{ Magnum::Vector3{ 0.0f, 0.0f, scale }, {}, blue } };
}

ShMeshPr cube() { return Mesh::create(Magnum::Primitives::cubeSolid()); }

ShMeshPr sphere(unsigned subdivisions) { return Mesh::create(Magnum::Primitives::icosphereSolid(subdivisions)); }

ShMeshPr plane() { return Mesh::create(Magnum::Primitives::planeSolid()); }

ShMeshPr grid(const Magnum::Vector2i& subdivisions) { return Mesh::create(Magnum::Primitives::grid3DWireframe(subdivisions)); }

ShMeshPr axes(float scale) {
    const std::vector<Vertex> verts = axes_vertices(scale);
    return Mesh::create({ verts.data(), verts.size() }, {}, Magnum::MeshPrimitive::Lines);
}

} // namespace smg::primitives
