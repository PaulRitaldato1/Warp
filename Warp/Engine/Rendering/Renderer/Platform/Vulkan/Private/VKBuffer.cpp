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
	m_size = static_cast<u64>(desc.numElements) * desc.stride;
	DYNAMIC_ASSERT(m_size > 0, "VKBuffer: buffer size must be > 0");

	VkBufferUsageFlags usageFlags =
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT  |
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT   |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT   |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

	// All normal buffers are GPU-only. Data upload happens via staging buffers
	// through UploadData() which returns a PendingStagingUpload.
	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size        = m_size;
	bufferInfo.usage       = usageFlags;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationInfo allocationInfo = {};
	VK_CHECK(vmaCreateBuffer(m_allocator, &bufferInfo, &allocInfo,
	                         &m_buffer, &m_allocation, &allocationInfo),
	         "VKBuffer: vmaCreateBuffer failed");

	m_mappedPtr       = nullptr;
	m_isStagingBuffer = false;

	// Obtain device address for future uniform/push-descriptor use.
	VkBufferDeviceAddressInfo addrInfo = {};
	addrInfo.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	addrInfo.buffer = m_buffer;
	m_deviceAddress = vkGetBufferDeviceAddress(m_device, &addrInfo);
}

URef<VKBuffer> VKBuffer::CreateStagingBuffer(VmaAllocator allocator, VkDevice device, u64 size,
                                              VkBufferUsageFlags extraUsageFlags)
{
	URef<VKBuffer> staging = std::make_unique<VKBuffer>();
	staging->m_allocator      = allocator;
	staging->m_device         = device;
	staging->m_size           = size;
	staging->m_isStagingBuffer = true;

	VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | extraUsageFlags;

	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
	allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size        = size;
	bufferInfo.usage       = usageFlags;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationInfo allocationInfo = {};
	VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &allocInfo,
	                         &staging->m_buffer, &staging->m_allocation, &allocationInfo),
	         "VKBuffer: staging vmaCreateBuffer failed");

	staging->m_mappedPtr = allocationInfo.pMappedData;

	if (usageFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
	{
		VkBufferDeviceAddressInfo addrInfo = {};
		addrInfo.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		addrInfo.buffer = staging->m_buffer;
		staging->m_deviceAddress = vkGetBufferDeviceAddress(device, &addrInfo);
	}
	else
	{
		staging->m_deviceAddress = 0;
	}

	return staging;
}

// ---------------------------------------------------------------------------
// Buffer overrides
// ---------------------------------------------------------------------------

PendingStagingUpload VKBuffer::UploadData(const void* data, size_t size)
{
	DYNAMIC_ASSERT(m_buffer, "VKBuffer::UploadData: buffer not initialized");
	DYNAMIC_ASSERT(data,     "VKBuffer::UploadData: data is null");
	DYNAMIC_ASSERT(size <= m_size, "VKBuffer::UploadData: size exceeds buffer capacity");
	DYNAMIC_ASSERT(!m_isStagingBuffer, "VKBuffer::UploadData: cannot upload to a staging buffer");

	URef<VKBuffer> staging = CreateStagingBuffer(m_allocator, m_device, size);

	void* mapped = staging->Map();
	std::memcpy(mapped, data, size);
	// Staging buffers stay mapped (CPU_ONLY with MAPPED_BIT), no need to unmap.

	PendingStagingUpload upload;
	upload.stagingBuffer = std::move(staging);
	upload.destination   = this;
	upload.size          = static_cast<u64>(size);
	return upload;
}

void* VKBuffer::Map()
{
	if (m_mappedPtr)
	{
		return m_mappedPtr;
	}

	DYNAMIC_ASSERT(m_isStagingBuffer, "VKBuffer::Map: only staging buffers can be mapped");

	void* ptr = nullptr;
	VK_CHECK(vmaMapMemory(m_allocator, m_allocation, &ptr),
	         "VKBuffer::Map: vmaMapMemory failed");
	m_mappedPtr = ptr;
	return ptr;
}

void VKBuffer::Unmap()
{
	if (m_isStagingBuffer && m_mappedPtr)
	{
		vmaUnmapMemory(m_allocator, m_allocation);
		m_mappedPtr = nullptr;
	}
}

#endif // WARP_BUILD_VK
