#ifdef WARP_BUILD_VK

#include <Rendering/Renderer/Platform/Vulkan/VKUploadBuffer.h>
#include <Debugging/Assert.h>

VKUploadBuffer::~VKUploadBuffer()
{
	Cleanup();
}

void VKUploadBuffer::InitializeWithAllocator(VmaAllocator allocator, VkDevice device)
{
	DYNAMIC_ASSERT(allocator, "VKUploadBuffer: allocator is null");
	DYNAMIC_ASSERT(device,    "VKUploadBuffer: device is null");
	m_allocator = allocator;
	m_device    = device;
}

void VKUploadBuffer::Initialize(u64 size, u32 framesInFlight)
{
	m_size = size;
	m_ringBuffer.Create(framesInFlight, static_cast<u32>(size));

	VkBufferUsageFlags extraFlags =
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

	m_backingBuffer = VKBuffer::CreateStagingBuffer(m_allocator, m_device, size, extraFlags);

	m_mapped  = static_cast<u8*>(m_backingBuffer->Map());
	m_gpuBase = m_backingBuffer->GetDeviceAddress();

	DYNAMIC_ASSERT(m_mapped, "VKUploadBuffer: failed to map backing buffer");
}

UploadAllocation VKUploadBuffer::Alloc(u64 size, u64 alignment)
{
	// The ring buffer inserts alignment padding internally so offset is already
	// aligned on return — no post-hoc rounding needed here.
	u32 offset = 0;
	bool ok = m_ringBuffer.Alloc(static_cast<u32>(size), &offset, static_cast<u32>(alignment));
	DYNAMIC_ASSERT(ok, "VKUploadBuffer::Alloc: upload buffer is full");

	UploadAllocation alloc;
	alloc.cpuPtr  = m_mapped + offset;
	alloc.gpuAddr = m_gpuBase + offset;
	alloc.offset  = static_cast<u64>(offset);
	alloc.size    = size;
	return alloc;
}

void VKUploadBuffer::OnBeginFrame()
{
	m_ringBuffer.OnBeginFrame();
}

void VKUploadBuffer::Cleanup()
{
	m_backingBuffer.reset();
	m_mapped  = nullptr;
	m_gpuBase = 0;
}

Buffer* VKUploadBuffer::GetBackingBuffer()
{
	return m_backingBuffer.get();
}

#endif // WARP_BUILD_VK
