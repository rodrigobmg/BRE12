#pragma once

#include <DirectXMath.h>

#include <MathUtils\MathUtils.h>

class Camera {
public:
	Camera() = default;
	~Camera() = default;
	Camera(const Camera&) = delete;
	const Camera& operator=(const Camera&) = delete;
	Camera(Camera&&) = delete;
	Camera& operator=(Camera&&) = delete;

	__forceinline DirectX::XMFLOAT3 GetPosition3f() const noexcept { return mPosition; }
	__forceinline DirectX::XMFLOAT4 GetPosition4f() const noexcept { return DirectX::XMFLOAT4(mPosition.x, mPosition.y, mPosition.z, 1.0f); }
	__forceinline void SetPosition(const DirectX::XMFLOAT3& v) noexcept { mPosition = v; }
		
	void SetFrustum(const float verticalFieldOfView,
					const float aspectRatio, 
					const float nearPlaneZ, 
					const float farPlaneZ) noexcept;

	void SetLookAndUpVectors(const DirectX::XMFLOAT3& cameraPosition, 
							 const DirectX::XMFLOAT3& targetPosition, 
							 const DirectX::XMFLOAT3& upVector) noexcept;

	__forceinline const DirectX::XMFLOAT4X4& GetViewMatrix() const noexcept { return mViewMatrix; }
	__forceinline const DirectX::XMFLOAT4X4& GetInverseViewMatrix() const noexcept { return mInverseViewMatrix; }
	__forceinline const DirectX::XMFLOAT4X4& GetProjectionMatrix() const noexcept { return mProjectionMatrix; }
	__forceinline const DirectX::XMFLOAT4X4& GetInverseProjectionMatrix() const noexcept { return mInverseProjectionMatrix; }
	
	// If distance is positive, then we
	// will strafe left / walk forward.
	// Otherwise, we will strafe right / walk backward.
	void Strafe(const float distance) noexcept;
	void Walk(const float distance) noexcept;

	void Pitch(const float angleInRadians) noexcept;
	void RotateY(const float angleInRadians) noexcept;

	void UpdateViewMatrix() noexcept;

private:
	DirectX::XMFLOAT3 mPosition = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 mRightVector = { 1.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 mUpVector = { 0.0f, 1.0f, 0.0f };
	DirectX::XMFLOAT3 mLookVector = { 0.0f, 0.0f, 1.0f };
	DirectX::XMFLOAT3 mVelocityVector{ 0.0f, 0.0f, 0.0f };

	DirectX::XMFLOAT4X4 mViewMatrix{ MathUtils::GetIdentity4x4Matrix() };
	DirectX::XMFLOAT4X4 mInverseViewMatrix{ MathUtils::GetIdentity4x4Matrix() };
	DirectX::XMFLOAT4X4 mProjectionMatrix{ MathUtils::GetIdentity4x4Matrix() };
	DirectX::XMFLOAT4X4 mInverseProjectionMatrix{ MathUtils::GetIdentity4x4Matrix() };
};