#include "AmbientLightPass.h"

#include <d3d12.h>

#include <CommandListExecutor/CommandListExecutor.h>
#include <DescriptorManager\RenderTargetDescriptorManager.h>
#include <DXUtils\d3dx12.h>
#include <ResourceManager\ResourceManager.h>
#include <ResourceStateManager\ResourceStateManager.h>
#include <Utils\DebugUtils.h>

namespace {
	void CreateResourceAndRenderTargetView(
		const D3D12_RESOURCE_STATES resourceinitialState,
		const wchar_t* resourceName,
		Microsoft::WRL::ComPtr<ID3D12Resource>& resource,
		D3D12_CPU_DESCRIPTOR_HANDLE& resourceRenderTargetView) noexcept 
	{		
		// Set shared buffers properties
		D3D12_RESOURCE_DESC resourceDescriptor = {};
		resourceDescriptor.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDescriptor.Alignment = 0U;
		resourceDescriptor.Width = SettingsManager::sWindowWidth;
		resourceDescriptor.Height = SettingsManager::sWindowHeight;
		resourceDescriptor.DepthOrArraySize = 1U;
		resourceDescriptor.MipLevels = 0U;
		resourceDescriptor.SampleDesc.Count = 1U;
		resourceDescriptor.SampleDesc.Quality = 0U;
		resourceDescriptor.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resourceDescriptor.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		resourceDescriptor.Format = DXGI_FORMAT_R16_UNORM;

		D3D12_CLEAR_VALUE clearValue{ resourceDescriptor.Format, 0.0f, 0.0f, 0.0f, 0.0f };
		resource.Reset();
		
		CD3DX12_HEAP_PROPERTIES heapProperties{ D3D12_HEAP_TYPE_DEFAULT };

		// Create buffer resource
		ID3D12Resource* resourcePtr = &ResourceManager::CreateCommittedResource(
			heapProperties, 
			D3D12_HEAP_FLAG_NONE, 
			resourceDescriptor, 
			resourceinitialState,
			&clearValue,
			resourceName);
		resource = Microsoft::WRL::ComPtr<ID3D12Resource>(resourcePtr);
		
		// Create render target view	
		D3D12_RENDER_TARGET_VIEW_DESC rtvDescriptor{};
		rtvDescriptor.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvDescriptor.Format = resourceDescriptor.Format;
		RenderTargetDescriptorManager::CreateRenderTargetView(*resource.Get(), rtvDescriptor, &resourceRenderTargetView);
	}
}

void AmbientLightPass::Init(
	ID3D12Resource& baseColorMetalMaskBuffer,
	ID3D12Resource& normalSmoothnessBuffer,
	ID3D12Resource& depthBuffer,
	const D3D12_CPU_DESCRIPTOR_HANDLE& renderTargetView) noexcept
{
	ASSERT(ValidateData() == false);

	AmbientLightCmdListRecorder::InitSharedPSOAndRootSignature();
	AmbientOcclusionCmdListRecorder::InitSharedPSOAndRootSignature();
	BlurCmdListRecorder::InitSharedPSOAndRootSignature();

	// Create ambient accessibility buffer and blur buffer
	CreateResourceAndRenderTargetView(
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		L"Ambient Accessibility Buffer",
		mAmbientAccessibilityBuffer, 
		mAmbientAccessibilityBufferRenderTargetView);

	D3D12_CPU_DESCRIPTOR_HANDLE blurBufferRenderTargetView;
	CreateResourceAndRenderTargetView(
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		L"Blur Buffer",
		mBlurBuffer, 
		blurBufferRenderTargetView);
	
	// Initialize ambient occlusion recorder
	mAmbientOcclusionRecorder.reset(new AmbientOcclusionCmdListRecorder());
	mAmbientOcclusionRecorder->Init(
		normalSmoothnessBuffer,		
		depthBuffer,
		mAmbientAccessibilityBufferRenderTargetView);

	// Initialize blur recorder
	mBlurRecorder.reset(new BlurCmdListRecorder());
	mBlurRecorder->Init(
		*mAmbientAccessibilityBuffer.Get(),
		blurBufferRenderTargetView);

	// Initialize ambient light recorder
	mAmbientLightRecorder.reset(new AmbientLightCmdListRecorder());
	mAmbientLightRecorder->Init(
		baseColorMetalMaskBuffer,		
		*mBlurBuffer.Get(),
		renderTargetView);

	ASSERT(ValidateData());
}

void AmbientLightPass::Execute(const FrameCBuffer& frameCBuffer) noexcept {
	ASSERT(ValidateData());

	const std::uint32_t taskCount{ 5U };
	CommandListExecutor::Get().ResetExecutedCommandListCount();

	ExecuteBeginTask();
	mAmbientOcclusionRecorder->RecordAndPushCommandLists(frameCBuffer);

	ExecuteMiddleTask();
	mBlurRecorder->RecordAndPushCommandLists();

	ExecuteFinalTask();
	mAmbientLightRecorder->RecordAndPushCommandLists();

	// Wait until all previous tasks command lists are executed
	while (CommandListExecutor::Get().GetExecutedCommandListCount() < taskCount) {
		Sleep(0U);
	}
}

bool AmbientLightPass::ValidateData() const noexcept {
	const bool b =
		mAmbientOcclusionRecorder.get() != nullptr &&
		mAmbientLightRecorder.get() != nullptr &&
		mAmbientAccessibilityBuffer.Get() != nullptr &&
		mBlurBuffer.Get() != nullptr;

	return b;
}

void AmbientLightPass::ExecuteBeginTask() noexcept {
	ASSERT(ValidateData());

	// Check resource states:
	// Ambient accesibility buffer was used as pixel shader resource by blur shader.
	// Blur buffer was used as pixel shader resource by ambient light shader
	ASSERT(ResourceStateManager::GetResourceState(*mAmbientAccessibilityBuffer.Get()) == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	ASSERT(ResourceStateManager::GetResourceState(*mBlurBuffer.Get()) == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	ID3D12GraphicsCommandList& commandList = mBeginCommandListPerFrame.ResetWithNextCommandAllocator(nullptr);

	CD3DX12_RESOURCE_BARRIER barriers[]
	{
		ResourceStateManager::ChangeResourceStateAndGetBarrier(
			*mAmbientAccessibilityBuffer.Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET),
	};
	const std::uint32_t barrierCount = _countof(barriers);
	ASSERT(barrierCount == 1UL);
	commandList.ResourceBarrier(barrierCount, barriers);

	float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	commandList.ClearRenderTargetView(mAmbientAccessibilityBufferRenderTargetView, clearColor, 0U, nullptr);

	CHECK_HR(commandList.Close());
	CommandListExecutor::Get().AddCommandList(commandList);
}

void AmbientLightPass::ExecuteMiddleTask() noexcept {
	ASSERT(ValidateData());

	// Check resource states:
	// Ambient accesibility buffer was used as render target resource by ambient accesibility shader.
	// Blur buffer was used as pixel shader resource by ambient light shader
	ASSERT(ResourceStateManager::GetResourceState(*mAmbientAccessibilityBuffer.Get()) == D3D12_RESOURCE_STATE_RENDER_TARGET);
	ASSERT(ResourceStateManager::GetResourceState(*mBlurBuffer.Get()) == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	ID3D12GraphicsCommandList& commandList = mMiddleCommandListPerFrame.ResetWithNextCommandAllocator(nullptr);

	CD3DX12_RESOURCE_BARRIER barriers[]
	{
		ResourceStateManager::ChangeResourceStateAndGetBarrier(
			*mAmbientAccessibilityBuffer.Get(), 
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),

		ResourceStateManager::ChangeResourceStateAndGetBarrier(
			*mBlurBuffer.Get(), 
			D3D12_RESOURCE_STATE_RENDER_TARGET),
	};
	const std::uint32_t barrierCount = _countof(barriers);
	ASSERT(barrierCount == 2UL);
	commandList.ResourceBarrier(barrierCount, barriers);

	CHECK_HR(commandList.Close());
	CommandListExecutor::Get().AddCommandList(commandList);
}

void AmbientLightPass::ExecuteFinalTask() noexcept {
	ASSERT(ValidateData());

	// Check resource states:
	// Ambient accesibility buffer was used as pixel shader resource by blur shader.
	// Blur buffer was used as render target resource by blur shader
	ASSERT(ResourceStateManager::GetResourceState(*mAmbientAccessibilityBuffer.Get()) == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	ASSERT(ResourceStateManager::GetResourceState(*mBlurBuffer.Get()) == D3D12_RESOURCE_STATE_RENDER_TARGET);

	ID3D12GraphicsCommandList& commandList = mFinalCommandListPerFrame.ResetWithNextCommandAllocator(nullptr);
	
	CD3DX12_RESOURCE_BARRIER barriers[]
	{
		ResourceStateManager::ChangeResourceStateAndGetBarrier(*mBlurBuffer.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
	};
	const std::uint32_t barrierCount = _countof(barriers);
	ASSERT(barrierCount == 1UL);
	commandList.ResourceBarrier(barrierCount, barriers);

	CHECK_HR(commandList.Close());
	CommandListExecutor::Get().AddCommandList(commandList);
}