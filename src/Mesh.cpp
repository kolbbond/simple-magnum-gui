#include "Mesh.hh"

#include <Corrade/Containers/Array.h>
#include <Magnum/GL/Buffer.h>
#include <Magnum/MeshTools/Compile.h>
#include <Magnum/Shaders/GenericGL.h>
#include <Magnum/Trade/MeshData.h>

namespace smg {

Mesh::Mesh(Magnum::GL::Mesh&& mesh, const Bounds& bounds) : _mesh{ std::move(mesh) }, _bounds{ bounds } {}

Bounds compute_bounds(const Magnum::Trade::MeshData& data) {
    Bounds b;
    const Corrade::Containers::Array<Magnum::Vector3> positions = data.positions3DAsArray();
    for(const Magnum::Vector3& p : positions) b.expand(p);
    return b;
}

ShMeshPr Mesh::create(const Magnum::Trade::MeshData& data) {
    Magnum::GL::Mesh glmesh = Magnum::MeshTools::compile(data);
    return std::make_shared<Mesh>(std::move(glmesh), compute_bounds(data));
}

ShMeshPr Mesh::create(Corrade::Containers::ArrayView<const Vertex> verts,
    Corrade::Containers::ArrayView<const Magnum::UnsignedInt> indices,
    Magnum::MeshPrimitive primitive) {

    Magnum::GL::Buffer vertexBuffer;
    vertexBuffer.setData(verts);

    Magnum::GL::Mesh glmesh;
    glmesh.setPrimitive(primitive).addVertexBuffer(std::move(vertexBuffer),
        0,
        Magnum::Shaders::GenericGL3D::Position{},
        Magnum::Shaders::GenericGL3D::Normal{},
        Magnum::Shaders::GenericGL3D::Color3{});

    if(!indices.isEmpty()) {
        Magnum::GL::Buffer indexBuffer;
        indexBuffer.setData(indices);
        glmesh.setCount(static_cast<Magnum::Int>(indices.size()))
            .setIndexBuffer(std::move(indexBuffer), 0, Magnum::MeshIndexType::UnsignedInt);
    } else {
        glmesh.setCount(static_cast<Magnum::Int>(verts.size()));
    }

    return std::make_shared<Mesh>(std::move(glmesh), compute_bounds(verts));
}

} // namespace smg
