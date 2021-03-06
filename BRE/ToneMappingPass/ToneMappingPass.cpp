#include "ToneMappingPass.h"

#include <d3d12.h>
#include <DirectXColors.h>

#include <CommandListExecutor/CommandListExecutor.h>
#include <DXUtils/d3dx12.h>
#include <ResourceManager\ResourceManager.h>
#include <ResourceStateManager\ResourceStateManager.h>
#include <ShaderUtils\CBuffers.h>
#include <Utils\DebugUtils.h>

void ToneMappingPass::Init(
	ID3D12Resource& inputColorBuffer,
	ID3D12Resource& outputColorBuffer,
	const D3D12_CPU_DESCRIPTOR_HANDLE& renderTargetView) noexcept 
{
	ASSERT(IsDataValid() == false);
	
	mInputColorBuffer = &inputColorBuffer;
	mOutputColorBuffer = &outputColorBuffer;

	ToneMappingCmdListRecorder::InitSharedPSOAndRootSignature();

	mCommandListRecorder.reset(new ToneMappingCmdListRecorder());
	mCommandListRecorder->Init(*mInputColorBuffer, renderTargetView);

	ASSERT(IsDataValid());
}

void ToneMappingPass::Execute() noexcept {
	ASSERT(IsDataValid());

	ExecuteBeginTask();

	CommandListExecutor::Get().ResetExecutedCommandListCount();
	mCommandListRecorder->RecordAndPushCommandLists();

	// Wait until all previous tasks command lists are executed
	while (CommandListExecutor::Get().GetExecutedCommandListCount() < 1) {
		Sleep(0U);
	}
}

bool ToneMappingPass::IsDataValid() const noexcept {
	const bool b =
		mCommandListRecorder.get() != nullptr &&
		mInputColorBuffer != nullptr &&
		mOutputColorBuffer != nullptr;

	return b;
}

void ToneMappingPass::ExecuteBeginTask() noexcept {
	ASSERT(IsDataValid());

	// Check resource states:
	// - Input color buffer was used as render target in previous pass
	// - Output color buffer was used as pixel shader resource in previous pass
	ASSERT(ResourceStateManager::GetResourceState(*mInputColorBuffer) == D3D12_RESOURCE_STATE_RENDER_TARGET);
	ASSERT(ResourceStateManager::GetResourceState(*mOutputColorBuffer) == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	ID3D12GraphicsCommandList& commandList = mCommandListPerFrame.ResetWithNextCommandAllocator(nullptr);

	CD3DX12_RESOURCE_BARRIER barriers[]
	{
		ResourceStateManager::ChangeResourceStateAndGetBarrier(*mInputColorBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
		ResourceStateManager::ChangeResourceStateAndGetBarrier(*mOutputColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET),
	};
	commandList.ResourceBarrier(_countof(barriers), barriers);

	CHECK_HR(commandList.Close());
	CommandListExecutor::Get().ExecuteCommandListAndWaitForCompletion(commandList);
}