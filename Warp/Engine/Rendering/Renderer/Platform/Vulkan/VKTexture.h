#pragma once

#ifdef WARP_BUILD_VK

#include <Rendering/Renderer/Texture.h>
#include <Rendering/Renderer/DescriptorHandle.h>
#include <Rendering/Renderer/Platform/Vulkan/VKCommon.h>
#include <vk_mem_alloc.h>

// ---------------------------------------------------------------------------
// VKTexture — GPU image backed by VMA.
// The VkImageView is exposed via GetDescriptorHandle() for binding as a
// render target, depth attachment, or shader resource.
// ---------------------------------------------------------------------------

class VKTexture : public Texture
{
public:
	~VKTexture() override;

	void InitializeWithAllocator(VmaAllocator allocator, VkDevice device);

	void  Init(const TextureDesc& desc) override;
	void  Cleanup()                     override;
	void* GetNativeHandle()       const override { return (void*)m_image; }
	u32   GetWidth()              const override { return m_width;  }
	u32   GetHeight()             const override { return m_height; }
	u32   GetDepth()              const override { return m_depth;  }
	u32   GetMipLevels()          const override { return m_mipLevels; }
	TextureFormat GetFormat()     const override { return m_format; }

	// Texture interface — all three return the same VkImageView; usage is
	// determined by the pipeline/render pass, not by separate view types.
	DescriptorHandle GetRTV() const override { return GetDescriptorHandle(); }
	DescriptorHandle GetSRV() const override { return GetDescriptorHandle(); }
	DescriptorHandle GetDSV() const override { return GetDescriptorHandle(); }

	VkImage     GetNativeImage() const { return m_image; }
	VkImageView GetNativeView()  const { return m_view;  }

	// Returns a DescriptorHandle with ptr = (u64)VkImageView and correct extent.
	DescriptorHandle GetDescriptorHandle() const;

	// Layout tracking (used by VKCommandList::TransitionTexture)
	VkImageLayout GetCurrentLayout() const { return m_currentLayout; }
	void SetCurrentLayout(VkImageLayout l) { m_currentLayout = l; }

private:
	VmaAllocator  m_allocator     = VK_NULL_HANDLE;
	VkDevice      m_device        = VK_NULL_HANDLE;
	VkImage       m_image         = VK_NULL_HANDLE;
	VkImageView   m_view          = VK_NULL_HANDLE;
	VmaAllocation m_allocation    = VK_NULL_HANDLE;
	VkImageLayout m_currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	u32           m_width     = 0;
	u32           m_height    = 0;
	u32           m_depth     = 1;
	u32           m_mipLevels = 1;
	TextureFormat m_format    = TextureFormat::RGBA8;
	TextureUsage  m_usage     = TextureUsage::Sampled;
	TextureType   m_type      = TextureType::Texture2D;
};

#endif // WARP_BUILD_VK
