#pragma once

#include <d3d12.h>
#include <dxgi1_4.h>
#include <tbb/task.h>

#include <CommandManager\CommandListPerFrame.h>
#include <Camera/Camera.h>
#include <GeometryPass\GeometryPass.h>
#include <LightingPass\LightingPass.h>
#include <PostProcesspass\PostProcesspass.h>
#include <SettingsManager\SettingsManager.h>
#include <SkyBoxPass\SkyBoxPass.h>
#include <ShaderUtils\CBuffers.h>
#include <ToneMappingPass\ToneMappingPass.h>
#include <Timer/Timer.h>

class CommandListExecutor;
class Scene;

// Initializes passes (geometry, light, skybox, etc) based on a Scene.
// Steps:
// - Use RenderManager::Create() to create and spawn and instance. 
// - When you want to terminate this task, you should call RenderManager::Terminate()
class RenderManager : public tbb::task {
public:
	// Preconditions:
	// - Create() must be called once
	static RenderManager& Create(Scene& scene) noexcept;

	~RenderManager() = default;
	RenderManager(const RenderManager&) = delete;
	const RenderManager& operator=(const RenderManager&) = delete;
	RenderManager(RenderManager&&) = delete;
	RenderManager& operator=(RenderManager&&) = delete;

	void Terminate() noexcept;

private:
	explicit RenderManager(Scene& scene);

	// Called when tbb::task is spawned
	tbb::task* execute() final override;

	static RenderManager* sRenderManager;

	void InitPasses(Scene& scene) noexcept;

	void CreateFrameBuffersAndRenderTargetViews() noexcept;

	void CreateDepthStencilBufferAndView() noexcept;

	void CreateIntermediateColorBufferAndRenderTargetView(		
		const D3D12_RESOURCE_STATES initialState,
		const wchar_t* resourceName,
		Microsoft::WRL::ComPtr<ID3D12Resource>& buffer,
		D3D12_CPU_DESCRIPTOR_HANDLE& renderTargetView) noexcept;
	
	ID3D12Resource* CurrentFrameBuffer() const noexcept {
		ASSERT(mSwapChain != nullptr);
		return mFrameBuffers[mSwapChain->GetCurrentBackBufferIndex()].Get();
	}

	D3D12_CPU_DESCRIPTOR_HANDLE CurrentFrameBufferCpuDesc() const noexcept {
		return mFrameBufferRenderTargetViews[mSwapChain->GetCurrentBackBufferIndex()];
	}

	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilCpuDesc() const noexcept {
		return mDepthBufferRenderTargetView;
	}

	void ExecuteFinalPass();

	void FlushCommandQueue() noexcept;
	void SignalFenceAndPresent() noexcept;

	Microsoft::WRL::ComPtr<IDXGISwapChain3> mSwapChain{ nullptr };
				
	// Fences data for synchronization purposes.
	ID3D12Fence* mFence{ nullptr };
	std::uint32_t mCurrentQueuedFrameIndex{ 0U };
	std::uint64_t mFenceValueByQueuedFrameIndex[SettingsManager::sQueuedFrameCount]{ 0UL };
	std::uint64_t mCurrentFenceValue{ 0UL };

	// Passes
	GeometryPass mGeometryPass;
	LightingPass mLightingPass;
	SkyBoxPass mSkyBoxPass;
	ToneMappingPass mToneMappingPass;
	PostProcessPass mPostProcessPass;

	CommandListPerFrame mFinalCommandListPerFrame;
	
	Microsoft::WRL::ComPtr<ID3D12Resource> mFrameBuffers[SettingsManager::sSwapChainBufferCount];
	D3D12_CPU_DESCRIPTOR_HANDLE mFrameBufferRenderTargetViews[SettingsManager::sSwapChainBufferCount]{ 0UL };

	ID3D12Resource* mDepthBuffer{ nullptr };
	D3D12_CPU_DESCRIPTOR_HANDLE mDepthBufferRenderTargetView{ 0UL };

	// Buffers used for intermediate computations.
	// They are used as render targets (light pass) or pixel shader resources (post processing passes)
	Microsoft::WRL::ComPtr<ID3D12Resource> mIntermediateColorBuffer1;
	D3D12_CPU_DESCRIPTOR_HANDLE mIntermediateColorBuffer1RenderTargetView;
	Microsoft::WRL::ComPtr<ID3D12Resource> mIntermediateColorBuffer2;
	D3D12_CPU_DESCRIPTOR_HANDLE mIntermediateColorBuffer2RenderTargetView;

	// We cache it here, as is is used by most passes.
	FrameCBuffer mFrameCBuffer;

	Camera mCamera;
	Timer mTimer;
	
	// When it is true, master render thread is destroyed.
	bool mTerminate{ false };
};
