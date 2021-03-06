#include "AmbientLightCmdListRecorder.h"

#include <DirectXMath.h>

#include <CommandListExecutor\CommandListExecutor.h>
#include <DescriptorManager\CbvSrvUavDescriptorManager.h>
#include <PSOManager/PSOManager.h>
#include <RootSignatureManager\RootSignatureManager.h>
#include <ShaderManager\ShaderManager.h>
#include <Utils/DebugUtils.h>

// Root Signature:
// "DescriptorTable(SRV(t0), SRV(t1), visibility = SHADER_VISIBILITY_PIXEL)" 0 -> BaseColor_MetalMaskTexture texture, AmbientAccessibility texture

namespace {
	ID3D12PipelineState* sPSO{ nullptr };
	ID3D12RootSignature* sRootSignature{ nullptr };
}

void AmbientLightCmdListRecorder::InitSharedPSOAndRootSignature() noexcept {
	ASSERT(sPSO == nullptr);
	ASSERT(sRootSignature == nullptr);

	PSOManager::PSOCreationData psoData{};
	psoData.mBlendDescriptor = D3DFactory::GetAlwaysBlendDesc();
	psoData.mDepthStencilDescriptor = D3DFactory::GetDisabledDepthStencilDesc();

	psoData.mPixelShaderBytecode = ShaderManager::LoadShaderFileAndGetBytecode("AmbientLightPass/Shaders/AmbientLight/PS.cso");
	psoData.mVertexShaderBytecode = ShaderManager::LoadShaderFileAndGetBytecode("AmbientLightPass/Shaders/AmbientLight/VS.cso");

	ID3DBlob* rootSignatureBlob = &ShaderManager::LoadShaderFileAndGetBlob("AmbientLightPass/Shaders/AmbientLight/RS.cso");
	psoData.mRootSignature = &RootSignatureManager::CreateRootSignatureFromBlob(*rootSignatureBlob);
	sRootSignature = psoData.mRootSignature;

	psoData.mNumRenderTargets = 1U;
	psoData.mRenderTargetFormats[0U] = SettingsManager::sColorBufferFormat;
	for (std::size_t i = psoData.mNumRenderTargets; i < _countof(psoData.mRenderTargetFormats); ++i) {
		psoData.mRenderTargetFormats[i] = DXGI_FORMAT_UNKNOWN;
	}
	psoData.mPrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	sPSO = &PSOManager::CreateGraphicsPSO(psoData);

	ASSERT(sPSO != nullptr);
	ASSERT(sRootSignature != nullptr);
}

void AmbientLightCmdListRecorder::Init(
	ID3D12Resource& baseColorMetalMaskBuffer,
	ID3D12Resource& ambientAccessibilityBuffer,
	const D3D12_CPU_DESCRIPTOR_HANDLE& renderTargetView) noexcept
{
	ASSERT(ValidateData() == false);

	mRenderTargetView = renderTargetView;

	InitShaderResourceViews(baseColorMetalMaskBuffer, ambientAccessibilityBuffer);

	ASSERT(ValidateData());
}

void AmbientLightCmdListRecorder::RecordAndPushCommandLists() noexcept {
	ASSERT(ValidateData());
	ASSERT(sPSO != nullptr);
	ASSERT(sRootSignature != nullptr);

	ID3D12GraphicsCommandList& commandList = mCommandListPerFrame.ResetWithNextCommandAllocator(sPSO);

	commandList.RSSetViewports(1U, &SettingsManager::sScreenViewport);
	commandList.RSSetScissorRects(1U, &SettingsManager::sScissorRect);
	commandList.OMSetRenderTargets(1U, &mRenderTargetView, false, nullptr);

	ID3D12DescriptorHeap* heaps[] = { &CbvSrvUavDescriptorManager::GetDescriptorHeap() };
	commandList.SetDescriptorHeaps(_countof(heaps), heaps);

	commandList.SetGraphicsRootSignature(sRootSignature);
	commandList.SetGraphicsRootDescriptorTable(0U, mStartPixelShaderResourceView);

	commandList.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList.DrawInstanced(6U, 1U, 0U, 0U);

	commandList.Close();
	CommandListExecutor::Get().AddCommandList(commandList);
}

bool AmbientLightCmdListRecorder::ValidateData() const noexcept {
	const bool result =
		mRenderTargetView.ptr != 0UL &&
		mStartPixelShaderResourceView.ptr != 0UL;

	return result;
}

void AmbientLightCmdListRecorder::InitShaderResourceViews(
	ID3D12Resource& baseColorMetalMaskBuffer, 
	ID3D12Resource& ambientAccessibilityBuffer) noexcept 
{
	ASSERT(mStartPixelShaderResourceView.ptr == 0UL);

	ID3D12Resource* resources[] = 
	{
		&baseColorMetalMaskBuffer,
		&ambientAccessibilityBuffer
	};

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDescriptors[2U]{};
	
	// Create baseColor_metalMask buffer texture descriptor
	srvDescriptors[0].Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDescriptors[0].ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDescriptors[0].Texture2D.MostDetailedMip = 0;
	srvDescriptors[0].Texture2D.ResourceMinLODClamp = 0.0f;
	srvDescriptors[0].Format = baseColorMetalMaskBuffer.GetDesc().Format;
	srvDescriptors[0].Texture2D.MipLevels = baseColorMetalMaskBuffer.GetDesc().MipLevels;

	// Create ambient accessibility buffer texture descriptor
	srvDescriptors[1].Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDescriptors[1].ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDescriptors[1].Texture2D.MostDetailedMip = 0;
	srvDescriptors[1].Texture2D.ResourceMinLODClamp = 0.0f;
	srvDescriptors[1].Format = ambientAccessibilityBuffer.GetDesc().Format;
	srvDescriptors[1].Texture2D.MipLevels = ambientAccessibilityBuffer.GetDesc().MipLevels;

	ASSERT(_countof(resources) == _countof(srvDescriptors));

	mStartPixelShaderResourceView = 
		CbvSrvUavDescriptorManager::CreateShaderResourceViews(
			resources, 
			srvDescriptors, 
			_countof(resources));
}