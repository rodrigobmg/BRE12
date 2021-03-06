#include "SceneExecutor.h"

#include <memory>

#include <CommandManager\CommandAllocatorManager.h>
#include <CommandManager/CommandListManager.h>
#include <CommandManager\CommandQueueManager.h>
#include <CommandManager\FenceManager.h>
#include <DescriptorManager/CbvSrvUavDescriptorManager.h>
#include <DescriptorManager/DepthStencilDescriptorManager.h>
#include <DescriptorManager/RenderTargetDescriptorManager.h>
#include <DirectXManager\DirectXManager.h>
#include <Input/Keyboard.h>
#include <Input/Mouse.h>
#include <MaterialManager/MaterialManager.h>
#include <PSOManager\PSOManager.h>
#include <RenderManager/RenderManager.h>
#include <ResourceManager\ResourceManager.h>
#include <ResourceManager\UploadBufferManager.h>
#include <RootSignatureManager\RootSignatureManager.h>
#include <ShaderManager\ShaderManager.h>
#include <ShaderUtils\CBuffers.h>
#include <Utils\DebugUtils.h>

using namespace DirectX;

namespace {
	void InitSystems(const HINSTANCE moduleInstanceHandle) noexcept 
	{
		const HWND windowHandle = DirectXManager::GetWindowHandle();

		LPDIRECTINPUT8 directInput;
		CHECK_HR(DirectInput8Create(moduleInstanceHandle, 
									DIRECTINPUT_VERSION, 
									IID_IDirectInput8, 
									reinterpret_cast<LPVOID*>(&directInput), 
									nullptr));
		Keyboard::Create(*directInput, windowHandle);
		Mouse::Create(*directInput, windowHandle);

		CbvSrvUavDescriptorManager::Init();
		DepthStencilDescriptorManager::Init();
		RenderTargetDescriptorManager::Init();
		MaterialManager::Init();

		ShowCursor(false);
	}

	void FinalizeSystems() noexcept {
		CommandAllocatorManager::EraseAll();
		CommandListManager::EraseAll();
		CommandQueueManager::EraseAll();
		FenceManager::EraseAll();
		PSOManager::EraseAll();
		ResourceManager::EraseAll();
		RootSignatureManager::EraseAll();
		ShaderManager::EraseAll();
		UploadBufferManager::EraseAll();
	}

	void UpdateKeyboardAndMouse() noexcept {
		Keyboard::Get().Update();
		Mouse::Get().Update();
		if (Keyboard::Get().IsKeyDown(DIK_ESCAPE)) {
			PostQuitMessage(0);
		}
	}

	// Runs program until Escape key is pressed.
	std::int32_t RunMessageLoop() noexcept {
		MSG message{ nullptr };
		while (message.message != WM_QUIT) {
			if (PeekMessage(&message, nullptr, 0U, 0U, PM_REMOVE)) {
				TranslateMessage(&message);
				DispatchMessage(&message);
			}
			else {
				UpdateKeyboardAndMouse();
			}
		}

		return static_cast<std::int32_t>(message.wParam);
	}
}

using namespace DirectX;

SceneExecutor::~SceneExecutor() {
	ASSERT(mRenderManager != nullptr);
	mRenderManager->Terminate();
	mTaskSchedulerInit.terminate();

	FinalizeSystems();
}

void SceneExecutor::Execute() noexcept {
	RunMessageLoop();
}

SceneExecutor::SceneExecutor(HINSTANCE moduleInstanceHandle, Scene* scene)
	: mTaskSchedulerInit()
	, mScene(scene)
{
	ASSERT(scene != nullptr);
	DirectXManager::Init(moduleInstanceHandle);
	InitSystems(moduleInstanceHandle);
	mRenderManager = &RenderManager::Create(*mScene.get());
}