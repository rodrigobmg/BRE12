#include "Mesh.h"

#include <assimp/scene.h>

#include <Utils/DebugUtils.h>

using namespace DirectX;

namespace {
	void CalculateTangentArray(GeometryGenerator::MeshData& meshData, const std::size_t triangleCount) noexcept {
		const std::size_t vertexCount{ meshData.mVertices.size() };
		XMFLOAT3 *tan1 = new XMFLOAT3[vertexCount * 2U];
		XMFLOAT3 *tan2 = tan1 + vertexCount;
		ZeroMemory(tan1, vertexCount * sizeof(XMFLOAT3) * 2U);
		std::size_t baseIndex{ 0UL };
		for (long a = 0; a < triangleCount; ++a) {
			const std::size_t i1{ meshData.mIndices32[baseIndex] };
			const std::size_t i2{ meshData.mIndices32[baseIndex + 1U] };
			const std::size_t i3{ meshData.mIndices32[baseIndex + 2U] };

			const XMFLOAT3& v1 = meshData.mVertices[i1].mPosition; 
			const XMFLOAT3& v2 = meshData.mVertices[i2].mPosition;
			const XMFLOAT3& v3 = meshData.mVertices[i3].mPosition;

			const XMFLOAT2& w1 = meshData.mVertices[i1].mUV;
			const XMFLOAT2& w2 = meshData.mVertices[i2].mUV;
			const XMFLOAT2& w3 = meshData.mVertices[i3].mUV;

			const float x1{ v2.x - v1.x };
			const float x2{ v3.x - v1.x };
			const float y1{ v2.y - v1.y };
			const float y2{ v3.y - v1.y };
			const float z1{ v2.z - v1.z };
			const float z2{ v3.z - v1.z };

			const float s1{ w2.x - w1.x };
			const float s2{ w3.x - w1.x };
			const float t1{ w2.y - w1.y };
			const float t2{ w3.y - w1.y };

			const float r{ 1.0f / (s1 * t2 - s2 * t1) };
			const XMFLOAT3 sdir((t2 * x1 - t1 * x2) * r, (t2 * y1 - t1 * y2) * r, (t2 * z1 - t1 * z2) * r);
			const XMFLOAT3 tdir((s1 * x2 - s2 * x1) * r, (s1 * y2 - s2 * y1) * r, (s1 * z2 - s2 * z1) * r);

			XMStoreFloat3(&tan1[i1], XMVectorAdd(XMLoadFloat3(&tan1[i1]), XMLoadFloat3(&sdir)));
			XMStoreFloat3(&tan1[i2], XMVectorAdd(XMLoadFloat3(&tan1[i2]), XMLoadFloat3(&sdir)));
			XMStoreFloat3(&tan1[i3], XMVectorAdd(XMLoadFloat3(&tan1[i3]), XMLoadFloat3(&sdir)));

			XMStoreFloat3(&tan2[i1], XMVectorAdd(XMLoadFloat3(&tan2[i1]), XMLoadFloat3(&tdir)));
			XMStoreFloat3(&tan2[i2], XMVectorAdd(XMLoadFloat3(&tan2[i2]), XMLoadFloat3(&tdir)));
			XMStoreFloat3(&tan2[i3], XMVectorAdd(XMLoadFloat3(&tan2[i3]), XMLoadFloat3(&tdir)));

			baseIndex += 3;
		}
	}

	void CreateVertexAndIndexBufferData(
		VertexAndIndexBufferCreator::VertexBufferData& vertexBufferData,
		VertexAndIndexBufferCreator::IndexBufferData& indexBufferData,
		const GeometryGenerator::MeshData& meshData,
		ID3D12GraphicsCommandList& commandList,
		Microsoft::WRL::ComPtr<ID3D12Resource>& uploadVertexBuffer,
		Microsoft::WRL::ComPtr<ID3D12Resource>& uploadIndexBuffer) noexcept 
	{
		ASSERT(vertexBufferData.IsDataValid() == false);
		ASSERT(indexBufferData.IsDataValid() == false);

		// Create vertex buffer
		VertexAndIndexBufferCreator::BufferCreationData vertexBufferParams(
			meshData.mVertices.data(), 
			static_cast<std::uint32_t>(meshData.mVertices.size()), 
			sizeof(GeometryGenerator::Vertex));

		VertexAndIndexBufferCreator::CreateVertexBuffer(
			commandList, 
			vertexBufferParams, 
			vertexBufferData, 
			uploadVertexBuffer);

		// Create index buffer
		VertexAndIndexBufferCreator::BufferCreationData indexBufferParams(
			meshData.mIndices32.data(), 
			static_cast<std::uint32_t>(meshData.mIndices32.size()), 
			sizeof(std::uint32_t));

		VertexAndIndexBufferCreator::CreateIndexBuffer(
			commandList, 
			indexBufferParams, 
			indexBufferData, 
			uploadIndexBuffer);

		ASSERT(vertexBufferData.IsDataValid());
		ASSERT(indexBufferData.IsDataValid());
	}
}

Mesh::Mesh(
	const aiMesh& mesh, 
	ID3D12GraphicsCommandList& cmdList,
	Microsoft::WRL::ComPtr<ID3D12Resource>& uploadVertexBuffer,
	Microsoft::WRL::ComPtr<ID3D12Resource>& uploadIndexBuffer) 
{
	GeometryGenerator::MeshData meshData;

	// Positions and Normals
	const std::size_t numVertices{ mesh.mNumVertices };
	ASSERT(numVertices > 0U);
	ASSERT(mesh.HasNormals());
	meshData.mVertices.resize(numVertices);
	for (std::uint32_t i = 0U; i < numVertices; ++i) {
		meshData.mVertices[i].mPosition = XMFLOAT3(reinterpret_cast<const float*>(&mesh.mVertices[i]));
		meshData.mVertices[i].mNormal = XMFLOAT3(reinterpret_cast<const float*>(&mesh.mNormals[i]));
	}
	
	// Texture Coordinates (if any)
	if (mesh.HasTextureCoords(0U)) {
		ASSERT(mesh.GetNumUVChannels() == 1U);
		const aiVector3D* aiTextureCoordinates{ mesh.mTextureCoords[0U] };
		ASSERT(aiTextureCoordinates != nullptr);
		for (std::uint32_t i = 0U; i < numVertices; i++) {
			meshData.mVertices[i].mUV = XMFLOAT2(reinterpret_cast<const float*>(&aiTextureCoordinates[i]));
		}
	}
	
	// Indices
	ASSERT(mesh.HasFaces());
	const std::uint32_t numFaces{ mesh.mNumFaces };
	for (std::uint32_t i = 0U; i < numFaces; ++i) {
		const aiFace* face = &mesh.mFaces[i];
		ASSERT(face != nullptr);
		// We only allow triangles
		ASSERT(face->mNumIndices == 3U);

		meshData.mIndices32.push_back(face->mIndices[0U]);
		meshData.mIndices32.push_back(face->mIndices[1U]);
		meshData.mIndices32.push_back(face->mIndices[2U]);
	}

	// Tangents
	if (mesh.HasTangentsAndBitangents()) {
		for (std::uint32_t i = 0U; i < numVertices; ++i) {
			meshData.mVertices[i].mTangent = XMFLOAT3(reinterpret_cast<const float*>(&mesh.mTangents[i]));
		}
	}
	else {
		CalculateTangentArray(meshData, mesh.mNumFaces);
	}

	CreateVertexAndIndexBufferData(
		mVertexBufferData, 
		mIndexBufferData, 
		meshData, 
		cmdList, 
		uploadVertexBuffer, 
		uploadIndexBuffer);

	ASSERT(mVertexBufferData.IsDataValid());
	ASSERT(mIndexBufferData.IsDataValid());
}

Mesh::Mesh(
	const GeometryGenerator::MeshData& meshData, 
	ID3D12GraphicsCommandList& commandList,
	Microsoft::WRL::ComPtr<ID3D12Resource>& uploadVertexBuffer,
	Microsoft::WRL::ComPtr<ID3D12Resource>& uploadIndexBuffer) 
{
	CreateVertexAndIndexBufferData(
		mVertexBufferData,
		mIndexBufferData, 
		meshData, 
		commandList, 
		uploadVertexBuffer, 
		uploadIndexBuffer);

	ASSERT(mVertexBufferData.IsDataValid());
	ASSERT(mIndexBufferData.IsDataValid());
}