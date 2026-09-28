#pragma once

#ifdef WARP_BUILD_VK

#include <Rendering/Renderer/SwapChain.h>
#include <Rendering/Renderer/DescriptorHandle.h>
#include <Rendering/Renderer/Platform/Vulkan/VKCommon.h>

// ---------------------------------------------------------------------------
// VKSwapChain — wraps VkSwapchainKHR.
//
// Synchronisation is GPU side, so the CPU never waits here and frames overlap
// the way DXGI allows on D3D12. The renderer's per frame fences bound how far
// ahead the CPU runs.
//   TransitionToRenderTarget(): acquires with an image available semaphore, and
//                               has the frame's submit wait on it and signal the
//                               image's render finished semaphore.
//   TransitionToPresent():      ends the dynamic render pass, records a
//                               barrier COLOR_ATTACHMENT → PRESENT_SRC.
//   Present():                  presents, waiting on render finished.
// ---------------------------------------------------------------------------

class VKCommandList;
class VKCommandQueue;

class VKSwapChain : public SwapChain
{
public:
	~VKSwapChain() override;

	// Called from VKDevice::CreateSwapChain.
	// graphicsQueue is non-owning. Present goes through it, and its next submit
	// picks up the frame's acquire and present semaphores.
	void InitializeWithContext(VkInstance       instance,
	                           VkPhysicalDevice physDevice,
	                           VkDevice         device,
	                           VKCommandQueue*  graphicsQueue,
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

	// For a swap chain the surface no longer matches, as reported by acquire or
	// present. CreateSwapChain takes the surface's current extent.
	void Recreate();

	VkSemaphore CreateBinarySemaphore() const;

	VkInstance       m_instance    = VK_NULL_HANDLE;
	VkPhysicalDevice m_physDevice  = VK_NULL_HANDLE;
	VkDevice         m_device      = VK_NULL_HANDLE;
	VkQueue          m_presentQueue= VK_NULL_HANDLE;
	u32              m_presentFamily = 0;
	VkSurfaceKHR     m_surface     = VK_NULL_HANDLE;
	VkSwapchainKHR   m_swapchain   = VK_NULL_HANDLE;

	Vector<VkImage>       m_images;
	Vector<VkImageView>   m_imageViews;
	Vector<VkImageLayout> m_imageLayouts;     // current layout per image

	VKCommandQueue* m_graphicsQueue = nullptr;

	// Signaled by acquire, waited on by the frame's submit. One per frame in
	// flight, rotated by m_acquireSlot. Lives as long as the surface.
	Vector<VkSemaphore> m_imageAvailable;
	u32 m_acquireSlot = 0;

	// Signaled by the frame's submit, waited on by present. One per image, rebuilt
	// with the swap chain.
	Vector<VkSemaphore> m_renderFinished;

	// Present is skipped for a frame that never acquired.
	bool m_bImageAcquired = false;

	// Set when acquire or present reports SUBOPTIMAL or OUT_OF_DATE.
	bool m_bNeedsRecreate = false;

	u32             m_currentIndex = 0;
	u32             m_width        = 0;
	u32             m_height       = 0;
	SwapChainFormat m_format       = SwapChainFormat::BGRA8;
	bool            m_vsync        = false;
	u32             m_bufferCount  = 2; // requested image count, kept for rebuilds

	void* m_nativeWindow = nullptr; // GLFWwindow*, stored for resize
};

#endif // WARP_BUILD_VK
