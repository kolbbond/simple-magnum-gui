// static helper functions
#pragma once

#include <memory>
#include <armadillo>

#include "MagnumExternal/ImGui/imgui.h"
#include "Magnum/ImGuiIntegration/Context.h"
#include "Magnum/GL/Mesh.h"

#include "typedefs.hh"

namespace smg {
typedef std::shared_ptr<class Statics> ShStaticsPr;
using namespace Magnum;

class Statics {

	// properties
protected:
	// methods
public:
	// constructor
	Statics();

	// destructor
	~Statics();

	// factory
	static ShStaticsPr create();
	static void HelpMarker(const char* desc);

	// conversions
	static Vector3 conv2Vector3(arma::Col<fltp> Rn);
	static arma::Col<fltp> conv2arma(Vector3 Rn);

	// copy
    /*
	static GL::Mesh copyMesh(const GL::Mesh& originalMesh) {
		// Get vertex buffer data (Assuming we know the structure)
		GL::Buffer originalVertexBuffer = originalMesh.vertexBuffers()[0];
		Containers::Array<char> vertexData = originalVertexBuffer.data();

		// Get index buffer data if indexed
		GL::Buffer originalIndexBuffer;
		Containers::Array<char> indexData;
		bool isIndexed = originalMesh.isIndexed();

		if(isIndexed) {
			originalIndexBuffer = originalMesh.indexBuffer();
			indexData = originalIndexBuffer.data();
		}

		// Create new buffers with copied data
		GL::Buffer newVertexBuffer, newIndexBuffer;
		newVertexBuffer.setData(vertexData);

		if(isIndexed) {
			newIndexBuffer.setData(indexData);
		}

		// Create new mesh with copied data
		GL::Mesh newMesh;
		newMesh.setCount(originalMesh.count()).addVertexBuffer(std::move(newVertexBuffer), 0, Shaders::Flat3D::Position{});

		if(isIndexed) {
			newMesh.setIndexBuffer(std::move(newIndexBuffer), 0, GL::MeshIndexType::UnsignedInt);
		}

		return newMesh;
	}
    */
};

} // namespace smg
