#include "Scene.h"

#include <d3d12.h>

#include <CommandManager\CommandAllocatorManager.h>
#include <CommandManager\CommandListManager.h>
#include <CommandManager\FenceManager.h>
#include <Utils\DebugUtils.h>

// cmdQueue is used by derived classes
void Scene::Init(ID3D12CommandQueue& /*cmdQueue*/) noexcept {
	ASSERT(IsDataValid() == false);

	CommandAllocatorManager::Get().CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, mCommandAllocators);
	CommandListManager::Get().CreateCommandList(D3D12_COMMAND_LIST_TYPE_DIRECT, *mCommandAllocators, mCommandList);
	mCommandList->Close();
	FenceManager::Get().CreateFence(0U, D3D12_FENCE_FLAG_NONE, mFence);

	ASSERT(IsDataValid());
}

bool 
Scene::IsDataValid() const {
	const bool b =
		mCommandAllocators != nullptr &&
		mCommandList != nullptr &&
		mFence != nullptr;

	return b;
}