#include "PunctualLightCmdListRecorder.h"

#include <DirectXMath.h>

#include <CommandListExecutor\CommandListExecutor.h>
#include <DescriptorManager\CbvSrvUavDescriptorManager.h>
#include <LightingPass/PunctualLight.h>
#include <MathUtils/MathUtils.h>
#include <PSOManager/PSOManager.h>
#include <ResourceManager/UploadBufferManager.h>
#include <RootSignatureManager\RootSignatureManager.h>
#include <ShaderManager\ShaderManager.h>
#include <ShaderUtils\CBuffers.h>
#include <Utils/DebugUtils.h>

// Root Signature:
// "CBV(b0, visibility = SHADER_VISIBILITY_VERTEX), " \ 0 -> Frame CBuffer
// "DescriptorTable(SRV(t0), visibility = SHADER_VISIBILITY_VERTEX), " \ 1 -> Lights Buffer
// "CBV(b0, visibility = SHADER_VISIBILITY_GEOMETRY), " \ 2 -> Frame CBuffer
// "CBV(b1, visibility = SHADER_VISIBILITY_GEOMETRY), " \ 3 -> Immutable CBuffer
// "CBV(b0, visibility = SHADER_VISIBILITY_PIXEL), " \ 4 -> Frame CBuffer
// "DescriptorTable(SRV(t0), SRV(t1), SRV(t2), visibility = SHADER_VISIBILITY_PIXEL)" 5 -> Textures

namespace {
	ID3D12PipelineState* sPSO{ nullptr };
	ID3D12RootSignature* sRootSignature{ nullptr };
}

void PunctualLightCmdListRecorder::InitSharedPSOAndRootSignature() noexcept {
	ASSERT(sPSO == nullptr);
	ASSERT(sRootSignature == nullptr);

	PSOManager::PSOCreationData psoData{};
	psoData.mBlendDescriptor = D3DFactory::GetAlwaysBlendDesc();
	psoData.mDepthStencilDescriptor = D3DFactory::GetDisabledDepthStencilDesc();

	psoData.mGeometryShaderBytecode = ShaderManager::LoadShaderFileAndGetBytecode("LightingPass/Shaders/PunctualLight/GS.cso");
	psoData.mPixelShaderBytecode = ShaderManager::LoadShaderFileAndGetBytecode("LightingPass/Shaders/PunctualLight/PS.cso");
	psoData.mVertexShaderBytecode = ShaderManager::LoadShaderFileAndGetBytecode("LightingPass/Shaders/PunctualLight/VS.cso");

	ID3DBlob* rootSignatureBlob = &ShaderManager::LoadShaderFileAndGetBlob("LightingPass/Shaders/PunctualLight/RS.cso");
	psoData.mRootSignature = &RootSignatureManager::CreateRootSignatureFromBlob(*rootSignatureBlob);
	sRootSignature = psoData.mRootSignature;

	psoData.mNumRenderTargets = 1U;
	psoData.mRenderTargetFormats[0U] = SettingsManager::sColorBufferFormat;
	for (std::size_t i = psoData.mNumRenderTargets; i < _countof(psoData.mRenderTargetFormats); ++i) {
		psoData.mRenderTargetFormats[i] = DXGI_FORMAT_UNKNOWN;
	}
	psoData.mPrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	sPSO = &PSOManager::CreateGraphicsPSO(psoData);

	ASSERT(sPSO != nullptr);
	ASSERT(sRootSignature != nullptr);
}

void PunctualLightCmdListRecorder::Init(
	Microsoft::WRL::ComPtr<ID3D12Resource>* geometryBuffers,
	const std::uint32_t geometryBuffersCount,
	ID3D12Resource& depthBuffer,
	const void* lights,
	const std::uint32_t numLights) noexcept
{
	ASSERT(IsDataValid() == false);
	ASSERT(geometryBuffers != nullptr);
	ASSERT(0 < geometryBuffersCount && geometryBuffersCount < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);
	ASSERT(lights != nullptr);
	ASSERT(numLights > 0U);
	
	mNumLights = numLights;

	InitConstantBuffers();
	CreateLightBuffersAndViews(lights);
	InitShaderResourceViews(geometryBuffers, geometryBuffersCount, depthBuffer);
	
	ASSERT(IsDataValid());
}

void PunctualLightCmdListRecorder::InitShaderResourceViews(
	Microsoft::WRL::ComPtr<ID3D12Resource>* geometryBuffers,
	const std::uint32_t geometryBuffersCount,
	ID3D12Resource& depthBuffer) noexcept
{
	ASSERT(geometryBuffers != nullptr);
	ASSERT(0 < geometryBuffersCount && geometryBuffersCount < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);
	ASSERT(mNumLights > 0U);

	// Number of geometry buffers + depth buffer
	const std::uint32_t numResources = geometryBuffersCount + 1U;

	std::vector<D3D12_SHADER_RESOURCE_VIEW_DESC> srvDescriptors;
	srvDescriptors.reserve(numResources);

	std::vector<ID3D12Resource*> resources;
	resources.reserve(numResources);

	// Fill data for geometry buffers SRV descriptors
	for (std::uint32_t i = 0U; i < geometryBuffersCount; ++i) {
		ASSERT(geometryBuffers[i].Get() != nullptr);

		resources.push_back(geometryBuffers[i].Get());

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDescriptor{};
		srvDescriptor.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDescriptor.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDescriptor.Texture2D.MostDetailedMip = 0;
		srvDescriptor.Texture2D.ResourceMinLODClamp = 0.0f;
		srvDescriptor.Format = resources.back()->GetDesc().Format;
		srvDescriptor.Texture2D.MipLevels = resources.back()->GetDesc().MipLevels;
		srvDescriptors.emplace_back(srvDescriptor);
	}

	// Fill depth buffer SRV description
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDescriptor{};
	srvDescriptor.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDescriptor.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDescriptor.Texture2D.MostDetailedMip = 0;
	srvDescriptor.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDescriptor.Format = SettingsManager::sDepthStencilSRVFormat;
	srvDescriptor.Texture2D.MipLevels = depthBuffer.GetDesc().MipLevels;
	srvDescriptors.emplace_back(srvDescriptor);
	resources.push_back(&depthBuffer);

	// Create pixel shader SRV descriptors
	mStartPixelShaderResourceView =
		CbvSrvUavDescriptorManager::CreateShaderResourceViews(
			resources.data(),
			srvDescriptors.data(),
			numResources);
}

void PunctualLightCmdListRecorder::RecordAndPushCommandLists(const FrameCBuffer& frameCBuffer) noexcept {
	ASSERT(IsDataValid());
	ASSERT(sPSO != nullptr);
	ASSERT(sRootSignature != nullptr);
	ASSERT(mRenderTargetView.ptr != 0UL);

	// Update frame constants
	UploadBuffer& uploadFrameCBuffer(mFrameUploadCBufferPerFrame.GetNextFrameCBuffer());
	uploadFrameCBuffer.CopyData(0U, &frameCBuffer, sizeof(frameCBuffer));

	ID3D12GraphicsCommandList& commandList = mCommandListPerFrame.ResetWithNextCommandAllocator(sPSO);

	commandList.RSSetViewports(1U, &SettingsManager::sScreenViewport);
	commandList.RSSetScissorRects(1U, &SettingsManager::sScissorRect);
	commandList.OMSetRenderTargets(1U, &mRenderTargetView, false, nullptr);

	ID3D12DescriptorHeap* heaps[] = { &CbvSrvUavDescriptorManager::GetDescriptorHeap() };
	commandList.SetDescriptorHeaps(_countof(heaps), heaps);

	commandList.SetGraphicsRootSignature(sRootSignature);
	const D3D12_GPU_VIRTUAL_ADDRESS frameCBufferGpuVAddress(uploadFrameCBuffer.GetResource()->GetGPUVirtualAddress());
	const D3D12_GPU_VIRTUAL_ADDRESS immutableCBufferGpuVAddress(mImmutableUploadCBuffer->GetResource()->GetGPUVirtualAddress());
	commandList.SetGraphicsRootConstantBufferView(0U, frameCBufferGpuVAddress);
	commandList.SetGraphicsRootDescriptorTable(1U, mStartLightsBufferShaderResourceView);
	commandList.SetGraphicsRootConstantBufferView(2U, frameCBufferGpuVAddress);
	commandList.SetGraphicsRootConstantBufferView(3U, immutableCBufferGpuVAddress);
	commandList.SetGraphicsRootConstantBufferView(4U, frameCBufferGpuVAddress);
	commandList.SetGraphicsRootDescriptorTable(5U, mStartPixelShaderResourceView);
	
	commandList.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);	
	commandList.DrawInstanced(mNumLights, 1U, 0U, 0U);

	commandList.Close();
	CommandListExecutor::Get().AddCommandList(commandList);
}

bool PunctualLightCmdListRecorder::IsDataValid() const noexcept {
	return LightingPassCmdListRecorder::IsDataValid() && mStartPixelShaderResourceView.ptr != 0UL;
}

void PunctualLightCmdListRecorder::CreateLightBuffersAndViews(const void* lights) noexcept {
	ASSERT(mLightsUploadBuffer == nullptr);
	ASSERT(lights != nullptr);
	ASSERT(mNumLights != 0U);

	// Create lights buffer and fill it
	const std::size_t lightBufferElemSize{ sizeof(PunctualLight) };
	mLightsUploadBuffer = &UploadBufferManager::CreateUploadBuffer(lightBufferElemSize, mNumLights);
	const std::uint8_t* lightsPtr = reinterpret_cast<const std::uint8_t*>(lights);
	for (std::uint32_t i = 0UL; i < mNumLights; ++i) {
		mLightsUploadBuffer->CopyData(i, lightsPtr + sizeof(PunctualLight) * i, sizeof(PunctualLight));
	}

	// Create lights buffer SRV	descriptor
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDescriptor{};
	srvDescriptor.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDescriptor.Format = mLightsUploadBuffer->GetResource()->GetDesc().Format;
	srvDescriptor.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDescriptor.Buffer.FirstElement = 0UL;
	srvDescriptor.Buffer.NumElements = mNumLights;
	srvDescriptor.Buffer.StructureByteStride = sizeof(PunctualLight);
	mStartLightsBufferShaderResourceView = 
		CbvSrvUavDescriptorManager::CreateShaderResourceView(*mLightsUploadBuffer->GetResource(), srvDescriptor);
}

void PunctualLightCmdListRecorder::InitConstantBuffers() noexcept {
	const std::size_t immutableCBufferElemSize{ UploadBuffer::GetRoundedConstantBufferSizeInBytes(sizeof(ImmutableCBuffer)) };
	mImmutableUploadCBuffer = &UploadBufferManager::CreateUploadBuffer(immutableCBufferElemSize, 1U);
	ImmutableCBuffer immutableCBuffer;
	mImmutableUploadCBuffer->CopyData(0U, &immutableCBuffer, sizeof(immutableCBuffer));
}