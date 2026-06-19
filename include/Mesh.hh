// GL mesh wrapper: owns a GL::Mesh and its AABB
#pragma once

#include <memory>

#include <Corrade/Containers/ArrayView.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/Magnum.h>
#include <Magnum/Trade/Trade.h>

#include "SceneTypes.hh"

namespace smg {

typedef std::shared_ptr<class Mesh> ShMeshPr;

class Mesh {
public:
    Mesh(Magnum::GL::Mesh&& mesh, const Bounds& bounds);

    // compile from Magnum mesh data (e.g. Primitives::*Solid())
    static ShMeshPr create(const Magnum::Trade::MeshData& data);
    // upload interleaved smg::Vertex data
    static ShMeshPr create(Corrade::Containers::ArrayView<const Vertex> verts,
        Corrade::Containers::ArrayView<const Magnum::UnsignedInt> indices,
        Magnum::MeshPrimitive primitive);

    [[nodiscard]] Magnum::GL::Mesh& gl() { return _mesh; }
    [[nodiscard]] const Bounds& bounds() const { return _bounds; }

private:
    Magnum::GL::Mesh _mesh;
    Bounds _bounds;
};

// AABB over the position attribute of mesh data (headless)
[[nodiscard]] Bounds compute_bounds(const Magnum::Trade::MeshData& data);

} // namespace smg
