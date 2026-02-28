#pragma once

#ifdef WARP_BUILD_VK

#include <Rendering/Renderer/Buffer.h>
#include <Rendering/Renderer/Platform/Vulkan/VKCommon.h>
#include <vk_mem_alloc.h>

// ---------------------------------------------------------------------------
// VKBuffer — GPU buffer backed by VMA.
//   Dynamic  → VMA_MEMORY_USAGE_CPU_TO_GPU, persistently mapped
//   Static   → VMA_MEMORY_USAGE_GPU_ONLY
// ---------------------------------------------------------------------------

class VKBuffer : public Buffer
{
public:
	~VKBuffer() override;

	void InitializeWithAllocator(VmaAllocator allocator, VkDevice device);

	void  UploadData(const void* data, size_t size) override;
	void* Map()                                     override;
	void  Unmap()                                   override;
	u64   GetSize()         const                   override { return m_size; }
	void* GetNativeHandle() const                   override { return (void*)m_buffer; }

	VkBuffer        GetNativeBuffer()  const { return m_buffer; }
	VkDeviceAddress GetDeviceAddress() const { return m_deviceAddress; }

	void CreateBuffer(const BufferDesc& desc);

	// State tracking for buffer barriers
	VkAccessFlags2 GetCurrentAccess() const { return m_currentAccess; }
	void SetCurrentAccess(VkAccessFlags2 a) { m_currentAccess = a; }

	bool IsHostVisible() const { return m_mappedPtr != nullptr || IsDynamic(); }

private:
	VmaAllocator    m_allocator     = VK_NULL_HANDLE;
	VkDevice        m_device        = VK_NULL_HANDLE;
	VkBuffer        m_buffer        = VK_NULL_HANDLE;
	VmaAllocation   m_allocation    = VK_NULL_HANDLE;
	VkDeviceAddress m_deviceAddress = 0;
	u64             m_size          = 0;
	void*           m_mappedPtr     = nullptr;
	VkAccessFlags2  m_currentAccess = VK_ACCESS_2_NONE;
};

#endif // WARP_BUILD_VK
