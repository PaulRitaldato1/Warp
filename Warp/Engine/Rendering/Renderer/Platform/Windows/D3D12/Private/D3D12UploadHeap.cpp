#ifdef WARP_BUILD_DX12

#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12UploadBuffer.h>
#include <Debugging/Assert.h>
#include <Debugging/Logging.h>

void D3D12UploadBuffer::InitializeWithDevice(ID3D12Device* device, u64 size, u32 framesInFlight)
{
	DYNAMIC_ASSERT(device,           "D3D12UploadBuffer::InitializeWithDevice: device is null");
	DYNAMIC_ASSERT(size > 0,         "D3D12UploadBuffer::InitializeWithDevice: size must be > 0");
	DYNAMIC_ASSERT(framesInFlight > 0, "D3D12UploadBuffer::InitializeWithDevice: framesInFlight must be > 0");

	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type                  = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width              = size;
	resourceDesc.Height             = 1;
	resourceDesc.DepthOrArraySize   = 1;
	resourceDesc.MipLevels          = 1;
	resourceDesc.Format             = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count   = 1;
	resourceDesc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ThrowIfFailed(device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_resource)));

	// Persistently map — upload heaps stay mapped for their entire lifetime.
	D3D12_RANGE readRange = { 0, 0 }; // CPU never reads back from this buffer
	ThrowIfFailed(m_resource->Map(0, &readRange, &m_mappedPtr));

	m_gpuBase = m_resource->GetGPUVirtualAddress();
	m_size    = size;

	m_ringBuffer.Create(framesInFlight, static_cast<u32>(size));

	LOG_DEBUG("D3D12UploadBuffer initialized (" + std::to_string(size / (1024 * 1024)) + " MB)");
}

void D3D12UploadBuffer::Initialize(u64 /*size*/, u32 /*framesInFlight*/)
{
	// Use InitializeWithDevice — needs a device to create the GPU resource.
}

UploadAllocation D3D12UploadBuffer::Alloc(u64 size, u64 alignment)
{
	DYNAMIC_ASSERT(m_mappedPtr, "D3D12UploadBuffer::Alloc: buffer not initialized");

	u32 offset = 0;
	const bool ok = m_ringBuffer.Alloc(static_cast<u32>(size), &offset);
	DYNAMIC_ASSERT(ok, "D3D12UploadBuffer::Alloc: upload heap out of space");

	// Align the offset up to the requested alignment.
	const u64 aligned = (static_cast<u64>(offset) + alignment - 1) & ~(alignment - 1);

	UploadAllocation alloc;
	alloc.cpuPtr  = static_cast<u8*>(m_mappedPtr) + aligned;
	alloc.gpuAddr = m_gpuBase + aligned;
	alloc.offset  = aligned;
	alloc.size    = size;
	return alloc;
}

void D3D12UploadBuffer::OnBeginFrame()
{
	m_ringBuffer.OnBeginFrame();
}

void D3D12UploadBuffer::Cleanup()
{
	if (m_resource && m_mappedPtr)
	{
		m_resource->Unmap(0, nullptr);
		m_mappedPtr = nullptr;
	}
	m_resource.Reset();
}

#endif
