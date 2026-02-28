#pragma once

#ifdef WARP_BUILD_VK

#include <Rendering/Renderer/SwapChain.h>
#include <Rendering/Renderer/DescriptorHandle.h>
#include <Rendering/Renderer/Platform/Vulkan/VKCommon.h>

// ---------------------------------------------------------------------------
// VKSwapChain — wraps VkSwapchainKHR.
//
// Synchronisation model (simple / correct, not optimal):
//   TransitionToRenderTarget(): acquires the next image (blocking via fence),
//                               then records a barrier PRESENT → COLOR_ATTACHMENT.
//   TransitionToPresent():      ends the dynamic render pass, records a
//                               barrier COLOR_ATTACHMENT → PRESENT_SRC.
//   Present():                  waits for the graphics queue to be idle,
//                               then calls vkQueuePresentKHR.
// ---------------------------------------------------------------------------

class VKCommandList;

class VKSwapChain : public SwapChain
{
public:
	~VKSwapChain() override;

	// Called from VKDevice::CreateSwapChain.
	void InitializeWithContext(VkInstance    instance,
	                           VkPhysicalDevice physDevice,
	                           VkDevice         device,
	                           VkQueue          presentQueue,
	                           u32              presentFamilyIndex);

	void Initialize(const SwapChainDesc& desc) override;
	void Present()                             override;
	void Resize(u32 width, u32 height)         override;
	void Cleanup()                             override;

	void*           GetCurrentBackBuffer() const override { return (void*)m_images[m_currentIndex]; }
	SwapChainFormat GetFormat()            const override { return m_format; }
	u32             GetWidth()             const override { return m_width; }
	u32             GetHeight()            const override { return m_height; }

	DescriptorHandle GetCurrentRTV()                  const override;
	void TransitionToRenderTarget(CommandList& cmd)         override;
	void TransitionToPresent(CommandList& cmd)              override;

private:
	void CreateSwapChain(const SwapChainDesc& desc);
	void CreateImageViews();
	void DestroySwapChainResources();

	VkInstance       m_instance    = VK_NULL_HANDLE;
	VkPhysicalDevice m_physDevice  = VK_NULL_HANDLE;
	VkDevice         m_device      = VK_NULL_HANDLE;
	VkQueue          m_presentQueue= VK_NULL_HANDLE;
	u32              m_presentFamily = 0;
	VkSurfaceKHR     m_surface     = VK_NULL_HANDLE;
	VkSwapchainKHR   m_swapchain   = VK_NULL_HANDLE;

	Vector<VkImage>       m_images;
	Vector<VkImageView>   m_imageViews;
	Vector<VkFence>       m_acquireFences;    // one per image, for blocking acquire
	Vector<VkImageLayout> m_imageLayouts;     // current layout per image

	u32             m_currentIndex = 0;
	u32             m_width        = 0;
	u32             m_height       = 0;
	SwapChainFormat m_format       = SwapChainFormat::BGRA8;
	bool            m_vsync        = false;

	// Window handles — stored for resize
	void* m_nativeWindow  = nullptr;  // X11 Window (uintptr_t)
	void* m_nativeDisplay = nullptr;  // X11 Display*
};

#endif // WARP_BUILD_VK
