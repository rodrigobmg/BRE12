#include "UploadBuffer.h"

UploadBuffer::UploadBuffer(ID3D12Device& device, const std::size_t elemSize, const std::size_t elemCount)
	: mElemSize(elemSize)
{
	ASSERT(elemSize > 0);
	ASSERT(elemCount > 0);

	CD3DX12_HEAP_PROPERTIES heapProps{ D3D12_HEAP_TYPE_UPLOAD };
	CD3DX12_RESOURCE_DESC resDesc{ CD3DX12_RESOURCE_DESC::Buffer(mElemSize * elemCount) };
	CHECK_HR(device.CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mBuffer)));
	CHECK_HR(mBuffer->Map(0, nullptr, (void**)&mMappedData));
}

UploadBuffer::~UploadBuffer() {
	ASSERT(mBuffer);
	ASSERT(mElemSize > 0);
	mBuffer->Unmap(0, nullptr);
	mMappedData = nullptr;
}

void UploadBuffer::CopyData(const std::size_t elemIndex, void* srcData, const std::size_t srcDataSize) noexcept {
	ASSERT(srcData);
	memcpy(&mMappedData[elemIndex * mElemSize], srcData, srcDataSize);
}