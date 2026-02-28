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

	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size        = size;
	bufferInfo.usage       = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
	                         VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
	allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

	VmaAllocationInfo allocationInfo = {};
	VK_CHECK(vmaCreateBuffer(m_allocator, &bufferInfo, &allocInfo,
	                         &m_buffer, &m_allocation, &allocationInfo),
	         "VKUploadBuffer: vmaCreateBuffer failed");

	m_mapped = static_cast<u8*>(allocationInfo.pMappedData);
	DYNAMIC_ASSERT(m_mapped, "VKUploadBuffer: failed to map upload buffer");

	VkBufferDeviceAddressInfo addrInfo = {};
	addrInfo.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	addrInfo.buffer = m_buffer;
	m_gpuBase = vkGetBufferDeviceAddress(m_device, &addrInfo);
}

UploadAllocation VKUploadBuffer::Alloc(u64 size, u64 alignment)
{
	// Align size up to the requested alignment before asking the ring buffer.
	u64 alignedSize = (size + alignment - 1) & ~(alignment - 1);

	u32 offset = 0;
	bool ok = m_ringBuffer.Alloc(static_cast<u32>(alignedSize), &offset);
	DYNAMIC_ASSERT(ok, "VKUploadBuffer::Alloc: upload buffer is full");

	UploadAllocation alloc;
	alloc.cpuPtr  = m_mapped + offset;
	alloc.gpuAddr = m_gpuBase + offset;
	alloc.offset  = static_cast<u64>(offset);
	alloc.size    = alignedSize;
	return alloc;
}

void VKUploadBuffer::OnBeginFrame()
{
	m_ringBuffer.OnBeginFrame();
}

void VKUploadBuffer::Cleanup()
{
	if (m_buffer != VK_NULL_HANDLE)
	{
		vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
		m_buffer     = VK_NULL_HANDLE;
		m_allocation = VK_NULL_HANDLE;
		m_mapped     = nullptr;
	}
}

#endif // WARP_BUILD_VK
