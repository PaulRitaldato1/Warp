#pragma once

#ifdef WARP_BUILD_VK

#include <Rendering/Renderer/Buffer.h>
#include <Rendering/Renderer/Platform/Vulkan/VKCommon.h>
#include <vk_mem_alloc.h>

// ---------------------------------------------------------------------------
// VKBuffer — GPU buffer backed by VMA.
//   Normal buffers → VMA_MEMORY_USAGE_GPU_ONLY (not host-visible)
//   Staging buffers → VMA_MEMORY_USAGE_CPU_ONLY (host-visible, for upload)
// ---------------------------------------------------------------------------

class VKBuffer : public Buffer
{
public:
	~VKBuffer() override;

	void InitializeWithAllocator(VmaAllocator allocator, VkDevice device);
	void CreateBuffer(const BufferDesc& desc);

	PendingStagingUpload UploadData(const void* data, size_t size) override;
	void* Map()                                     override;
	void  Unmap()                                   override;
	u64   GetSize()         const                   override { return m_size; }
	void* GetNativeHandle() const                   override { return (void*)m_buffer; }

	VkBuffer        GetNativeBuffer()  const { return m_buffer; }
	VkDeviceAddress GetDeviceAddress() const { return m_deviceAddress; }

	// State tracking for buffer barriers
	VkAccessFlags2 GetCurrentAccess() const { return m_currentAccess; }
	void SetCurrentAccess(VkAccessFlags2 a) { m_currentAccess = a; }

	bool IsStagingBuffer() const { return m_isStagingBuffer; }

private:
	// Creates a staging-only buffer on CPU-visible memory.
	// Used internally by UploadData() to create the temporary staging resource.
	static URef<VKBuffer> CreateStagingBuffer(VmaAllocator allocator, VkDevice device, u64 size);

	VmaAllocator    m_allocator     = VK_NULL_HANDLE;
	VkDevice        m_device        = VK_NULL_HANDLE;
	VkBuffer        m_buffer        = VK_NULL_HANDLE;
	VmaAllocation   m_allocation    = VK_NULL_HANDLE;
	VkDeviceAddress m_deviceAddress = 0;
	u64             m_size          = 0;
	void*           m_mappedPtr     = nullptr;
	VkAccessFlags2  m_currentAccess = VK_ACCESS_2_NONE;
	bool            m_isStagingBuffer = false;
};

#endif // WARP_BUILD_VK
