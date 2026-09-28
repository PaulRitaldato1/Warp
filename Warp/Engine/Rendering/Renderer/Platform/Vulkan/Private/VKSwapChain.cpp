#include <string>
#include <vulkan/vulkan_core.h>
#ifdef WARP_BUILD_VK

#include <Rendering/Renderer/Platform/Vulkan/VKSwapChain.h>
#include <Rendering/Renderer/Platform/Vulkan/VKCommandList.h>
#include <Rendering/Renderer/Platform/Vulkan/VKCommandQueue.h>
#include <Rendering/Renderer/Platform/Vulkan/VKTranslate.h>
#include <Rendering/Window/Window.h>
#include <Debugging/Assert.h>
#include <Debugging/Logging.h>
#include <algorithm>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

VKSwapChain::~VKSwapChain()
{
	Cleanup();
}

void VKSwapChain::InitializeWithContext(VkInstance instance, VkPhysicalDevice physDevice, VkDevice device,
										VKCommandQueue* graphicsQueue, u32 presentFamilyIndex)
{
	DYNAMIC_ASSERT(instance, "VKSwapChain: instance is null");
	DYNAMIC_ASSERT(physDevice, "VKSwapChain: physDevice is null");
	DYNAMIC_ASSERT(device, "VKSwapChain: device is null");
	DYNAMIC_ASSERT(graphicsQueue, "VKSwapChain: graphicsQueue is null");

	m_instance		= instance;
	m_physDevice	= physDevice;
	m_device		= device;
	m_graphicsQueue = graphicsQueue;
	m_presentQueue	= graphicsQueue->GetNative();
	m_presentFamily = presentFamilyIndex;
}

// ---------------------------------------------------------------------------
// SwapChain interface
// ---------------------------------------------------------------------------

void VKSwapChain::Initialize(const SwapChainDesc& desc)
{
	m_width			= desc.Width;
	m_height		= desc.Height;
	m_format		= desc.Format;
	m_vsync			= desc.bUseVsync;
	m_bufferCount	= desc.BufferCount;
	m_nativeWindow	= desc.Window->GetNativeHandle();

	// GLFW picks the platform surface (Win32, X11 or Wayland) for whatever it is
	// running on, so there is no per-platform path here.
	DYNAMIC_ASSERT(m_nativeWindow, "VKSwapChain: window handle is null");
	VK_CHECK(glfwCreateWindowSurface(m_instance, static_cast<GLFWwindow*>(m_nativeWindow), nullptr, &m_surface),
			 "VKSwapChain: glfwCreateWindowSurface failed");

	// One per frame in flight. A frame's acquire semaphore is reused two frames
	// later, by which point the renderer has waited on that frame's fence, so its
	// submit has consumed the wait. BufferCount matches k_framesInFlight.
	m_imageAvailable.resize(m_bufferCount, VK_NULL_HANDLE);
	for (VkSemaphore& semaphore : m_imageAvailable)
	{
		semaphore = CreateBinarySemaphore();
	}

	CreateSwapChain(desc);
	CreateImageViews();

	LOG_DEBUG("VKSwapChain initialized ({}x{}, {} images)", m_width, m_height, m_images.size());
}

void VKSwapChain::CreateSwapChain(const SwapChainDesc& desc)
{
	// Query surface capabilities.
	VkSurfaceCapabilitiesKHR caps = {};
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physDevice, m_surface, &caps);

	// Pick image count.
	u32 imageCount = std::max(desc.BufferCount, caps.minImageCount);
	if (caps.maxImageCount > 0)
	{
		imageCount = std::min(imageCount, caps.maxImageCount);
	}

	// Choose surface format.
	u32 fmtCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(m_physDevice, m_surface, &fmtCount, nullptr);
	Vector<VkSurfaceFormatKHR> formats(fmtCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(m_physDevice, m_surface, &fmtCount, formats.data());

	VkFormat wantedFmt		  = ToVkFormat(TextureFormat::BGRA8);
	VkColorSpaceKHR wantedCS  = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	VkSurfaceFormatKHR chosen = formats[0];

	for (const VkSurfaceFormatKHR& f : formats)
	{
		if (f.format == wantedFmt && f.colorSpace == wantedCS)
		{
			chosen = f;
			break;
		}
	}

	// Choose present mode.
	u32 pmCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(m_physDevice, m_surface, &pmCount, nullptr);
	Vector<VkPresentModeKHR> presentModes(pmCount);
	vkGetPhysicalDeviceSurfacePresentModesKHR(m_physDevice, m_surface, &pmCount, presentModes.data());

	// MAILBOX does not uncap the frame rate — it avoids blocking the CPU but still
	// presents at refresh. IMMEDIATE is the counterpart to D3D12's ALLOW_TEARING.
	// Both are optional; only FIFO is guaranteed, so prefer in order.
	// Untested: Vulkan does not currently run on Windows and Linux has its own gaps.
	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR; // always available (vsync)
	if (!m_vsync)
	{
		const bool hasImmediate =
			std::find(presentModes.begin(), presentModes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != presentModes.end();
		const bool hasMailbox =
			std::find(presentModes.begin(), presentModes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != presentModes.end();

		if (hasImmediate)
		{
			presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
		}
		else if (hasMailbox)
		{
			presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
		}
	}

	// Clamp extent to surface caps.
	VkExtent2D extent = { m_width, m_height };
	if (caps.currentExtent.width != UINT32_MAX)
	{
		extent = caps.currentExtent;
	}
	extent.width  = std::clamp(extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
	extent.height = std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
	m_width		  = extent.width;
	m_height	  = extent.height;

	VkSwapchainCreateInfoKHR scInfo = {};
	scInfo.sType					= VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	scInfo.surface					= m_surface;
	scInfo.minImageCount			= imageCount;
	scInfo.imageFormat				= chosen.format;
	scInfo.imageColorSpace			= chosen.colorSpace;
	scInfo.imageExtent				= extent;
	scInfo.imageArrayLayers			= 1;
	scInfo.imageUsage				= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	scInfo.imageSharingMode			= VK_SHARING_MODE_EXCLUSIVE;
	scInfo.preTransform				= caps.currentTransform;
	scInfo.compositeAlpha			= VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	scInfo.presentMode				= presentMode;
	scInfo.clipped					= VK_TRUE;
	scInfo.oldSwapchain				= VK_NULL_HANDLE;

	VK_CHECK(vkCreateSwapchainKHR(m_device, &scInfo, nullptr, &m_swapchain),
			 "VKSwapChain: vkCreateSwapchainKHR failed");

	// Retrieve the actual images.
	u32 actualCount = 0;
	vkGetSwapchainImagesKHR(m_device, m_swapchain, &actualCount, nullptr);
	m_images.resize(actualCount);
	vkGetSwapchainImagesKHR(m_device, m_swapchain, &actualCount, m_images.data());
}

void VKSwapChain::CreateImageViews()
{
	const u32 count = static_cast<u32>(m_images.size());

	m_imageViews.resize(count, VK_NULL_HANDLE);
	m_imageLayouts.resize(count, VK_IMAGE_LAYOUT_UNDEFINED);

	// Per image, not per frame. Present waits on it, and the only proof that a
	// present has finished with its semaphore is getting the same image back from
	// acquire. Keyed by frame slot, it could be signaled again while still in use.
	m_renderFinished.resize(count, VK_NULL_HANDLE);
	for (VkSemaphore& semaphore : m_renderFinished)
	{
		semaphore = CreateBinarySemaphore();
	}

	for (u32 i = 0; i < count; ++i)
	{
		VkImageViewCreateInfo viewInfo			 = {};
		viewInfo.sType							 = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image							 = m_images[i];
		viewInfo.viewType						 = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format							 = ToVkFormat(TextureFormat::BGRA8);
		viewInfo.subresourceRange.aspectMask	 = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel	 = 0;
		viewInfo.subresourceRange.levelCount	 = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount	 = 1;

		VK_CHECK(vkCreateImageView(m_device, &viewInfo, nullptr, &m_imageViews[i]),
				 "VKSwapChain: vkCreateImageView failed");
	}
}

void VKSwapChain::DestroySwapChainResources()
{
	for (u32 i = 0; i < static_cast<u32>(m_imageViews.size()); ++i)
	{
		if (m_imageViews[i] != VK_NULL_HANDLE)
		{
			vkDestroyImageView(m_device, m_imageViews[i], nullptr);
		}
	}
	m_imageViews.clear();
	m_imageLayouts.clear();
	m_images.clear();

	for (VkSemaphore semaphore : m_renderFinished)
	{
		vkDestroySemaphore(m_device, semaphore, nullptr);
	}
	m_renderFinished.clear();

	if (m_swapchain != VK_NULL_HANDLE)
	{
		vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
		m_swapchain = VK_NULL_HANDLE;
	}
}

void VKSwapChain::Present()
{
	// A frame that never reached the lighting pass acquired nothing to present.
	if (!m_bImageAcquired)
	{
		return;
	}
	m_bImageAcquired = false;

	// The GPU waits for the frame's submit to signal this, so the CPU does not.
	VkPresentInfoKHR presentInfo   = {};
	presentInfo.sType			   = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores	   = &m_renderFinished[m_currentIndex];
	presentInfo.swapchainCount	   = 1;
	presentInfo.pSwapchains		   = &m_swapchain;
	presentInfo.pImageIndices	   = &m_currentIndex;

	VkResult result = vkQueuePresentKHR(m_presentQueue, &presentInfo);
	if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		// Rebuilt before the next acquire, rather than mid present.
		if (!m_bNeedsRecreate)
		{
			LOG_WARNING("VKSwapChain::Present: swap chain {}, recreating",
						result == VK_SUBOPTIMAL_KHR ? "suboptimal" : "out of date");
		}
		m_bNeedsRecreate = true;
		return;
	}

	VK_CHECK(result, "VKSwapChain::Present: vkQueuePresentKHR failed");
}

void VKSwapChain::Recreate()
{
	// Same size as before. CreateSwapChain replaces it with the surface's current
	// extent when the surface reports one.
	Resize(m_width, m_height);
	m_bNeedsRecreate = false;
}

void VKSwapChain::Resize(u32 width, u32 height)
{
	vkDeviceWaitIdle(m_device);
	DestroySwapChainResources();

	m_width	 = width;
	m_height = height;

	// Minimal desc for reconstruction.
	SwapChainDesc desc;
	desc.Width		 = width;
	desc.Height		 = height;
	desc.BufferCount = m_bufferCount;
	desc.Format		 = m_format;
	desc.bUseVsync	 = m_vsync;
	desc.Window		 = nullptr; // surface already created; window not needed for rebuild

	CreateSwapChain(desc);
	CreateImageViews();

	LOG_DEBUG("VKSwapChain resized: {}x{}", width, height);
}

void VKSwapChain::Cleanup()
{
	DestroySwapChainResources();

	for (VkSemaphore semaphore : m_imageAvailable)
	{
		vkDestroySemaphore(m_device, semaphore, nullptr);
	}
	m_imageAvailable.clear();

	if (m_surface != VK_NULL_HANDLE)
	{
		vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
		m_surface = VK_NULL_HANDLE;
	}
}

DescriptorHandle VKSwapChain::GetCurrentRTV() const
{
	DescriptorHandle h;
	h.ptr	 = static_cast<u64>(reinterpret_cast<uintptr_t>(m_imageViews[m_currentIndex]));
	h.width	 = m_width;
	h.height = m_height;
	return h;
}

VkSemaphore VKSwapChain::CreateBinarySemaphore() const
{
	VkSemaphoreCreateInfo info = {};
	info.sType				   = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkSemaphore semaphore = VK_NULL_HANDLE;
	VK_CHECK(vkCreateSemaphore(m_device, &info, nullptr, &semaphore), "VKSwapChain: vkCreateSemaphore failed");
	return semaphore;
}

void VKSwapChain::TransitionToRenderTarget(CommandList& cmd)
{
	if (m_bNeedsRecreate)
	{
		Recreate();
	}

	// Acquire returns the image index straight away and signals the semaphore once
	// the presentation engine has actually released the image. Only the GPU waits.
	VkSemaphore imageAvailable = m_imageAvailable[m_acquireSlot];
	VkResult result =
		vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, imageAvailable, VK_NULL_HANDLE, &m_currentIndex);

	// Nothing was acquired or signaled, so the same semaphore is safe to reuse.
	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		LOG_WARNING("VKSwapChain::TransitionToRenderTarget: swap chain out of date, recreating");
		Recreate();

		result =
			vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, imageAvailable, VK_NULL_HANDLE, &m_currentIndex);
	}

	// Suboptimal still acquired an image and will signal the semaphore, so this
	// frame uses it. The rebuild waits for the next acquire.
	if (result == VK_SUBOPTIMAL_KHR)
	{
		if (!m_bNeedsRecreate)
		{
			LOG_WARNING("VKSwapChain::TransitionToRenderTarget: swap chain suboptimal, recreating next frame");
		}
		m_bNeedsRecreate = true;
	}
	else
	{
		VK_CHECK(result, "VKSwapChain::TransitionToRenderTarget: vkAcquireNextImageKHR failed");
	}

	m_acquireSlot	 = (m_acquireSlot + 1) % static_cast<u32>(m_imageAvailable.size());
	m_bImageAcquired = true;

	// The frame's submit waits for the image only where it first writes it, so the
	// shadow, G-buffer and cull work ahead of this runs before the image is free.
	// It signals the image's render finished semaphore for Present to wait on.
	m_graphicsQueue->AddBinaryWait(imageAvailable, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
	m_graphicsQueue->AddBinarySignal(m_renderFinished[m_currentIndex]);

	// Transition the acquired image from UNDEFINED / PRESENT to COLOR_ATTACHMENT.
	VKCommandList& vkCmd = static_cast<VKCommandList&>(cmd);

	VkImageLayout oldLayout = m_imageLayouts[m_currentIndex];

	// Source stage matches the semaphore's wait stage, so the layout change is
	// ordered after the image is actually released.
	VkImageMemoryBarrier2 barrier = {};
	barrier.sType				  = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	barrier.srcStageMask		  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	barrier.srcAccessMask		  = 0;
	barrier.dstStageMask		  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	barrier.dstAccessMask		  = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	barrier.oldLayout			  = oldLayout;
	barrier.newLayout			  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	barrier.srcQueueFamilyIndex	  = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex	  = VK_QUEUE_FAMILY_IGNORED;
	barrier.image				  = m_images[m_currentIndex];
	barrier.subresourceRange	  = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

	VkDependencyInfo depInfo		= {};
	depInfo.sType					= VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depInfo.imageMemoryBarrierCount = 1;
	depInfo.pImageMemoryBarriers	= &barrier;

	vkCmdPipelineBarrier2(vkCmd.GetNative(), &depInfo);
	m_imageLayouts[m_currentIndex] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
}

void VKSwapChain::TransitionToPresent(CommandList& cmd)
{
	VKCommandList& vkCmd = static_cast<VKCommandList&>(cmd);

	// End the dynamic render pass before transitioning.
	vkCmd.EndCurrentRenderPass();

	VkImageMemoryBarrier2 barrier = {};
	barrier.sType				  = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	barrier.srcStageMask		  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	barrier.srcAccessMask		  = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	barrier.dstStageMask		  = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
	barrier.dstAccessMask		  = 0;
	barrier.oldLayout			  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	barrier.newLayout			  = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	barrier.srcQueueFamilyIndex	  = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex	  = VK_QUEUE_FAMILY_IGNORED;
	barrier.image				  = m_images[m_currentIndex];
	barrier.subresourceRange	  = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

	VkDependencyInfo depInfo		= {};
	depInfo.sType					= VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depInfo.imageMemoryBarrierCount = 1;
	depInfo.pImageMemoryBarriers	= &barrier;

	vkCmdPipelineBarrier2(vkCmd.GetNative(), &depInfo);
	m_imageLayouts[m_currentIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
}

#endif // WARP_BUILD_VK
