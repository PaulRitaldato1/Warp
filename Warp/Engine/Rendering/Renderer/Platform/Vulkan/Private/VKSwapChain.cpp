#include <string>
#ifdef WARP_BUILD_VK

#include <Rendering/Renderer/Platform/Vulkan/VKSwapChain.h>
#include <Rendering/Renderer/Platform/Vulkan/VKCommandList.h>
#include <Rendering/Renderer/Platform/Vulkan/VKTranslate.h>
#include <Rendering/Window/Window.h>
#include <Debugging/Assert.h>
#include <Debugging/Logging.h>
#include <algorithm>

VKSwapChain::~VKSwapChain()
{
	Cleanup();
}

void VKSwapChain::InitializeWithContext(VkInstance       instance,
                                        VkPhysicalDevice physDevice,
                                        VkDevice         device,
                                        VkQueue          presentQueue,
                                        u32              presentFamilyIndex)
{
	DYNAMIC_ASSERT(instance,    "VKSwapChain: instance is null");
	DYNAMIC_ASSERT(physDevice,  "VKSwapChain: physDevice is null");
	DYNAMIC_ASSERT(device,      "VKSwapChain: device is null");
	DYNAMIC_ASSERT(presentQueue,"VKSwapChain: presentQueue is null");

	m_instance      = instance;
	m_physDevice    = physDevice;
	m_device        = device;
	m_presentQueue  = presentQueue;
	m_presentFamily = presentFamilyIndex;
}

// ---------------------------------------------------------------------------
// SwapChain interface
// ---------------------------------------------------------------------------

void VKSwapChain::Initialize(const SwapChainDesc& desc)
{
	m_width         = desc.Width;
	m_height        = desc.Height;
	m_format        = desc.Format;
	m_vsync         = desc.bUseVsync;
	m_nativeWindow  = desc.Window->GetNativeHandle();
	m_nativeDisplay = desc.Window->GetNativeDisplay();

	// -------------------------------------------------------------------------
	// Create platform surface
	// -------------------------------------------------------------------------

#ifdef WARP_LINUX
	DYNAMIC_ASSERT(m_nativeDisplay, "VKSwapChain: X11 Display* is null");
	DYNAMIC_ASSERT(m_nativeWindow,  "VKSwapChain: X11 Window handle is null");

	VkXlibSurfaceCreateInfoKHR surfaceInfo = {};
	surfaceInfo.sType  = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
	surfaceInfo.dpy    = static_cast<Display*>(m_nativeDisplay);
	surfaceInfo.window = static_cast<::Window>(reinterpret_cast<uintptr_t>(m_nativeWindow));

	VK_CHECK(vkCreateXlibSurfaceKHR(m_instance, &surfaceInfo, nullptr, &m_surface),
	         "VKSwapChain: vkCreateXlibSurfaceKHR failed");
#else
	FATAL_ASSERT(false, "VKSwapChain: surface creation not implemented on this platform");
#endif

	CreateSwapChain(desc);
	CreateImageViews();

	LOG_DEBUG("VKSwapChain initialized (" + std::to_string(m_width) + "x" +
	          std::to_string(m_height) + ", " + std::to_string(m_images.size()) + " images)");
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

	VkFormat           wantedFmt = ToVkFormat(TextureFormat::BGRA8);
	VkColorSpaceKHR    wantedCS  = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	VkSurfaceFormatKHR chosen    = formats[0];

	for (const auto& f : formats)
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

	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR; // always available (vsync)
	if (!m_vsync)
	{
		for (auto pm : presentModes)
		{
			if (pm == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				presentMode = pm;
				break;
			}
		}
	}

	// Clamp extent to surface caps.
	VkExtent2D extent = { m_width, m_height };
	if (caps.currentExtent.width != UINT32_MAX)
	{
		extent = caps.currentExtent;
	}
	extent.width  = std::clamp(extent.width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
	extent.height = std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
	m_width  = extent.width;
	m_height = extent.height;

	VkSwapchainCreateInfoKHR scInfo = {};
	scInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	scInfo.surface          = m_surface;
	scInfo.minImageCount    = imageCount;
	scInfo.imageFormat      = chosen.format;
	scInfo.imageColorSpace  = chosen.colorSpace;
	scInfo.imageExtent      = extent;
	scInfo.imageArrayLayers = 1;
	scInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	scInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	scInfo.preTransform     = caps.currentTransform;
	scInfo.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	scInfo.presentMode      = presentMode;
	scInfo.clipped          = VK_TRUE;
	scInfo.oldSwapchain     = VK_NULL_HANDLE;

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
	m_acquireFences.resize(count, VK_NULL_HANDLE);
	m_imageLayouts.resize(count, VK_IMAGE_LAYOUT_UNDEFINED);

	for (u32 i = 0; i < count; ++i)
	{
		VkImageViewCreateInfo viewInfo = {};
		viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image                           = m_images[i];
		viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format                          = ToVkFormat(TextureFormat::BGRA8);
		viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel   = 0;
		viewInfo.subresourceRange.levelCount     = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount     = 1;

		VK_CHECK(vkCreateImageView(m_device, &viewInfo, nullptr, &m_imageViews[i]),
		         "VKSwapChain: vkCreateImageView failed");

		VkFenceCreateInfo fenceInfo = {};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

		VK_CHECK(vkCreateFence(m_device, &fenceInfo, nullptr, &m_acquireFences[i]),
		         "VKSwapChain: vkCreateFence failed");
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
		if (m_acquireFences[i] != VK_NULL_HANDLE)
		{
			vkDestroyFence(m_device, m_acquireFences[i], nullptr);
		}
	}
	m_imageViews.clear();
	m_acquireFences.clear();
	m_imageLayouts.clear();
	m_images.clear();

	if (m_swapchain != VK_NULL_HANDLE)
	{
		vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
		m_swapchain = VK_NULL_HANDLE;
	}
}

void VKSwapChain::Present()
{
	// Ensure all GPU work against this image is complete before presenting.
	vkQueueWaitIdle(m_presentQueue);

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType          = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains    = &m_swapchain;
	presentInfo.pImageIndices  = &m_currentIndex;

	VkResult result = vkQueuePresentKHR(m_presentQueue, &presentInfo);
	if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR && result != VK_ERROR_OUT_OF_DATE_KHR)
	{
		VK_CHECK(result, "VKSwapChain::Present: vkQueuePresentKHR failed");
	}
}

void VKSwapChain::Resize(u32 width, u32 height)
{
	vkDeviceWaitIdle(m_device);
	DestroySwapChainResources();

	m_width  = width;
	m_height = height;

	// Minimal desc for reconstruction.
	SwapChainDesc desc;
	desc.Width       = width;
	desc.Height      = height;
	desc.BufferCount = 2;
	desc.Format      = m_format;
	desc.bUseVsync   = m_vsync;
	desc.Window      = nullptr; // surface already created; window not needed for rebuild

	CreateSwapChain(desc);
	CreateImageViews();

	LOG_DEBUG("VKSwapChain resized: " + std::to_string(width) + "x" + std::to_string(height));
}

void VKSwapChain::Cleanup()
{
	DestroySwapChainResources();

	if (m_surface != VK_NULL_HANDLE)
	{
		vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
		m_surface = VK_NULL_HANDLE;
	}
}

DescriptorHandle VKSwapChain::GetCurrentRTV() const
{
	DescriptorHandle h;
	h.ptr    = static_cast<u64>(reinterpret_cast<uintptr_t>(m_imageViews[m_currentIndex]));
	h.width  = m_width;
	h.height = m_height;
	return h;
}

void VKSwapChain::TransitionToRenderTarget(CommandList& cmd)
{
	// Acquire the next swapchain image (blocking via fence).
	VkFence fence = m_acquireFences[m_currentIndex];
	vkResetFences(m_device, 1, &fence);

	VkResult result = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX,
	                                         VK_NULL_HANDLE, fence, &m_currentIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
	{
		return;
	}

	VK_CHECK(vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX),
	         "VKSwapChain::TransitionToRenderTarget: fence wait failed");

	// Transition the acquired image from UNDEFINED / PRESENT to COLOR_ATTACHMENT.
	VKCommandList& vkCmd = static_cast<VKCommandList&>(cmd);

	VkImageLayout oldLayout = m_imageLayouts[m_currentIndex];

	VkImageMemoryBarrier2 barrier = {};
	barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	barrier.srcStageMask        = (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED)
	                                  ? VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT
	                                  : VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	barrier.srcAccessMask       = 0;
	barrier.dstStageMask        = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	barrier.dstAccessMask       = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	barrier.oldLayout           = oldLayout;
	barrier.newLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image               = m_images[m_currentIndex];
	barrier.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

	VkDependencyInfo depInfo = {};
	depInfo.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depInfo.imageMemoryBarrierCount = 1;
	depInfo.pImageMemoryBarriers    = &barrier;

	vkCmdPipelineBarrier2(vkCmd.GetNative(), &depInfo);
	m_imageLayouts[m_currentIndex] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
}

void VKSwapChain::TransitionToPresent(CommandList& cmd)
{
	VKCommandList& vkCmd = static_cast<VKCommandList&>(cmd);

	// End the dynamic render pass before transitioning.
	vkCmd.EndCurrentRenderPass();

	VkImageMemoryBarrier2 barrier = {};
	barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	barrier.srcStageMask        = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	barrier.srcAccessMask       = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	barrier.dstStageMask        = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
	barrier.dstAccessMask       = 0;
	barrier.oldLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	barrier.newLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image               = m_images[m_currentIndex];
	barrier.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

	VkDependencyInfo depInfo = {};
	depInfo.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depInfo.imageMemoryBarrierCount = 1;
	depInfo.pImageMemoryBarriers    = &barrier;

	vkCmdPipelineBarrier2(vkCmd.GetNative(), &depInfo);
	m_imageLayouts[m_currentIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
}

#endif // WARP_BUILD_VK
