#pragma once

#ifdef WARP_BUILD_VK

#include <Rendering/Renderer/UploadBuffer.h>
#include <Rendering/Renderer/Platform/Vulkan/VKBuffer.h>
#include <Rendering/Renderer/Platform/Vulkan/VKCommon.h>
#include <vk_mem_alloc.h>

// ---------------------------------------------------------------------------
// VKUploadBuffer — host-visible ring buffer for per-frame constant data and
// as a source for CopyBuffer re-uploads.  Backed by a VKBuffer created as a
// staging buffer with UNIFORM_BUFFER | SHADER_DEVICE_ADDRESS | TRANSFER_SRC.
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
	Buffer*          GetBackingBuffer()                       override;

private:
	VmaAllocator      m_allocator = VK_NULL_HANDLE;
	VkDevice          m_device    = VK_NULL_HANDLE;
	URef<VKBuffer>    m_backingBuffer;
	u8*               m_mapped    = nullptr;
	u64               m_gpuBase   = 0;
};

#endif // WARP_BUILD_VK
