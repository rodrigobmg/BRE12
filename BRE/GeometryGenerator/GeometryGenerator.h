#pragma once

#include <DirectXMath.h>
#include <vector>

// To generate procedurally the geometry of 
// common mathematical objects.
//
// All triangles are generated "outward" facing.
namespace GeometryGenerator {
	struct Vertex {
		Vertex() = default;
		explicit Vertex(
			const DirectX::XMFLOAT3& position, 
			const DirectX::XMFLOAT3& normal, 
			const DirectX::XMFLOAT3& tangent, 
			const DirectX::XMFLOAT2& uv);

		~Vertex() = default;
		Vertex(const Vertex&) = default;
		Vertex(Vertex&&) = default;
		Vertex& operator=(Vertex&&) = default;

		DirectX::XMFLOAT3 mPosition = {0.0f, 0.0f, 0.0f};
        DirectX::XMFLOAT3 mNormal = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 mTangent = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT2 mUV = { 0.0f, 0.0f };
	};

	struct MeshData {
		std::vector<Vertex> mVertices;
        std::vector<std::uint32_t> mIndices32;
		std::vector<std::uint16_t>& GetIndices16() noexcept;

	private:
		std::vector<std::uint16_t> mIndices16{};
	};

	// Centered at the origin.
	// Preconditions:
	// - 
    void CreateBox(
		const float width, 
		const float height, 
		const float depth, 
		const std::uint32_t numSubdivisions, 
		MeshData& meshData) noexcept;

	// Centered at the origin.
	void CreateSphere(
		const float radius, 
		const std::uint32_t sliceCount, 
		const std::uint32_t stackCount, 
		MeshData& meshData) noexcept;

	// Centered at the origin.
	void CreateGeosphere(
		const float radius, 
		const std::uint32_t numSubdivisions, 
		MeshData& meshData) noexcept;

	// Creates a cylinder parallel to the y-axis, and centered about the origin.  
	// The bottom and top radius can vary to form various cone shapes rather than true
	// cylinders.  The slices and stacks parameters control the degree of tessellation.
	void CreateCylinder(
		const float bottomRadius, 
		const float topRadius, 
		const float height, 
		const std::uint32_t sliceCount, 
		const std::uint32_t stackCount, 
		MeshData& meshData) noexcept;

	// Creates an row x columns grid in the xz-plane with rows rows and columns columns, centered
	// at the origin with the specified width and depth.
	void CreateGrid(
		const float width, 
		const float depth, 
		const std::uint32_t rows, 
		const std::uint32_t columns, 
		MeshData& meshData) noexcept;
}

