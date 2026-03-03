#ifdef WARP_BUILD_DX12

#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12UploadBuffer.h>
#include <Debugging/Assert.h>
#include <Debugging/Logging.h>

void D3D12UploadBuffer::InitializeWithDevice(ID3D12Device* device, u64 size, u32 framesInFlight)
{
	DYNAMIC_ASSERT(device,             "D3D12UploadBuffer::InitializeWithDevice: device is null");
	DYNAMIC_ASSERT(size > 0,           "D3D12UploadBuffer::InitializeWithDevice: size must be > 0");
	DYNAMIC_ASSERT(framesInFlight > 0, "D3D12UploadBuffer::InitializeWithDevice: framesInFlight must be > 0");

	m_size = size;
	m_ringBuffer.Create(framesInFlight, static_cast<u32>(size));

	m_backingBuffer = D3D12Buffer::CreateStagingBuffer(device, size);

	// Persistently map — upload heaps stay mapped for their entire lifetime.
	m_mappedPtr = m_backingBuffer->Map();
	m_gpuBase   = m_backingBuffer->GetGPUAddress();

	LOG_DEBUG("D3D12UploadBuffer initialized ({} MB)", size / (1024 * 1024));
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
	if (m_backingBuffer && m_mappedPtr)
	{
		m_backingBuffer->Unmap();
		m_mappedPtr = nullptr;
	}
	m_backingBuffer.reset();
}

Buffer* D3D12UploadBuffer::GetBackingBuffer()
{
	return m_backingBuffer.get();
}

#endif
