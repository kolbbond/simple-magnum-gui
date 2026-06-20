#include "Primitives.hh"

#include <Magnum/GL/Mesh.h>
#include <Magnum/Primitives/Cube.h>
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

// XZ ground-plane wireframe grid, centered at origin, spanning [-1,1] x [-1,1].
// subdivisions.x() lines run parallel to X; subdivisions.y() lines parallel to Z.
// Each axis gets (subdivisions+1) lines, each line is two endpoint vertices.
std::vector<Vertex> grid_vertices(const Magnum::Vector2i& subdivisions, const Magnum::Color3& color) {
    std::vector<Vertex> verts;
    // lines parallel to X (vary Z, fix X extent)
    const int nx = subdivisions.x() + 1;
    for(int i = 0; i < nx; ++i) {
        const float z = (subdivisions.x() > 0) ? -1.0f + 2.0f * float(i) / float(subdivisions.x()) : 0.0f;
        verts.push_back(Vertex{ Magnum::Vector3{ -1.0f, 0.0f, z }, {}, color });
        verts.push_back(Vertex{ Magnum::Vector3{ 1.0f, 0.0f, z }, {}, color });
    }
    // lines parallel to Z (vary X, fix Z extent)
    const int nz = subdivisions.y() + 1;
    for(int i = 0; i < nz; ++i) {
        const float x = (subdivisions.y() > 0) ? -1.0f + 2.0f * float(i) / float(subdivisions.y()) : 0.0f;
        verts.push_back(Vertex{ Magnum::Vector3{ x, 0.0f, -1.0f }, {}, color });
        verts.push_back(Vertex{ Magnum::Vector3{ x, 0.0f, 1.0f }, {}, color });
    }
    return verts;
}

ShMeshPr cube() { return Mesh::create(Magnum::Primitives::cubeSolid()); }

ShMeshPr sphere(unsigned subdivisions) { return Mesh::create(Magnum::Primitives::icosphereSolid(subdivisions)); }

ShMeshPr plane() { return Mesh::create(Magnum::Primitives::planeSolid()); }

ShMeshPr grid(const Magnum::Vector2i& subdivisions, const Magnum::Color3& color) {
    const std::vector<Vertex> v = grid_vertices(subdivisions, color);
    return Mesh::create({ v.data(), v.size() }, {}, Magnum::MeshPrimitive::Lines);
}

ShMeshPr axes(float scale) {
    const std::vector<Vertex> verts = axes_vertices(scale);
    return Mesh::create({ verts.data(), verts.size() }, {}, Magnum::MeshPrimitive::Lines);
}

} // namespace smg::primitives
