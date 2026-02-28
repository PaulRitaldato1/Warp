#pragma once

#ifdef WARP_BUILD_VK

#include <Rendering/Renderer/UploadBuffer.h>
#include <Rendering/Renderer/Platform/Vulkan/VKCommon.h>
#include <vk_mem_alloc.h>

// ---------------------------------------------------------------------------
// VKUploadBuffer — host-visible / device-local ring buffer for per-frame
// constant data.  Backed by a single persistently-mapped VMA allocation.
// gpuAddr on each UploadAllocation is a VkDeviceAddress for future use with
// push descriptors; CPU writes are done via the mapped pointer.
// ---------------------------------------------------------------------------

class VKUploadBuffer : public UploadBuffer
{
public:
	~VKUploadBuffer() override;

	void InitializeWithAllocator(VmaAllocator allocator, VkDevice device);

	void             Initialize(u64 size, u32 framesInFlight) override;
	UploadAllocation Alloc(u64 size, u64 alignment = 256)     override;
	void             OnBeginFrame()                           override;
	void             Cleanup()                                override;

private:
	VmaAllocator  m_allocator  = VK_NULL_HANDLE;
	VkDevice      m_device     = VK_NULL_HANDLE;
	VkBuffer      m_buffer     = VK_NULL_HANDLE;
	VmaAllocation m_allocation = VK_NULL_HANDLE;
	u8*           m_mapped     = nullptr;
	u64           m_gpuBase    = 0;   // VkDeviceAddress of the backing buffer
};

#endif // WARP_BUILD_VK
