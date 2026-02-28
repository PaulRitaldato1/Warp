#ifdef WARP_BUILD_VK

#include <Rendering/Renderer/Platform/Vulkan/VKBuffer.h>
#include <Debugging/Assert.h>
#include <Debugging/Logging.h>
#include <cstring>

VKBuffer::~VKBuffer()
{
	if (m_buffer != VK_NULL_HANDLE)
	{
		vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
		m_buffer     = VK_NULL_HANDLE;
		m_allocation = VK_NULL_HANDLE;
	}
}

void VKBuffer::InitializeWithAllocator(VmaAllocator allocator, VkDevice device)
{
	DYNAMIC_ASSERT(allocator, "VKBuffer: allocator is null");
	DYNAMIC_ASSERT(device,    "VKBuffer: device is null");
	m_allocator = allocator;
	m_device    = device;
}

void VKBuffer::CreateBuffer(const BufferDesc& desc)
{
	m_usage = desc.usage;
	m_size  = static_cast<u64>(desc.numElements) * desc.stride;
	DYNAMIC_ASSERT(m_size > 0, "VKBuffer: buffer size must be > 0");

	VkBufferUsageFlags usageFlags =
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT  |
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT   |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT   |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

	VmaAllocationCreateInfo allocInfo = {};

	if (desc.usage == BufferUsage::Dynamic)
	{
		allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
		allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
	}
	else
	{
		allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		usageFlags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	}

	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size        = m_size;
	bufferInfo.usage       = usageFlags;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationInfo allocationInfo = {};
	VK_CHECK(vmaCreateBuffer(m_allocator, &bufferInfo, &allocInfo,
	                         &m_buffer, &m_allocation, &allocationInfo),
	         "VKBuffer: vmaCreateBuffer failed");

	if (desc.usage == BufferUsage::Dynamic)
	{
		m_mappedPtr = allocationInfo.pMappedData;
	}

	// Obtain device address for future uniform/push-descriptor use.
	VkBufferDeviceAddressInfo addrInfo = {};
	addrInfo.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	addrInfo.buffer = m_buffer;
	m_deviceAddress = vkGetBufferDeviceAddress(m_device, &addrInfo);

	// Upload initial data if provided.
	if (desc.initialData && m_mappedPtr)
	{
		std::memcpy(m_mappedPtr, desc.initialData, static_cast<size_t>(m_size));
	}
}

// ---------------------------------------------------------------------------
// Buffer overrides
// ---------------------------------------------------------------------------

void VKBuffer::UploadData(const void* data, size_t size)
{
	DYNAMIC_ASSERT(m_mappedPtr, "VKBuffer::UploadData: buffer is not host-visible");
	std::memcpy(m_mappedPtr, data, size);
}

void* VKBuffer::Map()
{
	if (m_mappedPtr)
	{
		return m_mappedPtr;
	}

	void* ptr = nullptr;
	VK_CHECK(vmaMapMemory(m_allocator, m_allocation, &ptr),
	         "VKBuffer::Map: vmaMapMemory failed");
	m_mappedPtr = ptr;
	return ptr;
}

void VKBuffer::Unmap()
{
	if (m_mappedPtr && !IsDynamic())
	{
		vmaUnmapMemory(m_allocator, m_allocation);
		m_mappedPtr = nullptr;
	}
	// Dynamic buffers stay persistently mapped — nothing to do.
}

#endif // WARP_BUILD_VK
