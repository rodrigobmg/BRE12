#include "EnvironmentLightPass.h"

#include <d3d12.h>

#include <ResourceManager\ResourceManager.h>
#include <Utils\DebugUtils.h>

void EnvironmentLightPass::Init(
	Microsoft::WRL::ComPtr<ID3D12Resource>* geometryBuffers,
	const std::uint32_t geometryBuffersCount,
	ID3D12Resource& depthBuffer,
	ID3D12Resource& diffuseIrradianceCubeMap,
	ID3D12Resource& specularPreConvolvedCubeMap,
	const D3D12_CPU_DESCRIPTOR_HANDLE& renderTargetView) noexcept
{
	ASSERT(ValidateData() == false);

	EnvironmentLightCmdListRecorder::InitSharedPSOAndRootSignature();

	mCommandListRecorder.reset(new EnvironmentLightCmdListRecorder());
	mCommandListRecorder->Init(
		geometryBuffers, 
		geometryBuffersCount,
		depthBuffer,		
		diffuseIrradianceCubeMap,
		specularPreConvolvedCubeMap,
		renderTargetView);

	ASSERT(ValidateData());
}

void EnvironmentLightPass::Execute(const FrameCBuffer& frameCBuffer) const noexcept {
	ASSERT(ValidateData());

	mCommandListRecorder->RecordAndPushCommandLists(frameCBuffer);
}

bool EnvironmentLightPass::ValidateData() const noexcept {
	const bool b = mCommandListRecorder.get() != nullptr;

	return b;
}