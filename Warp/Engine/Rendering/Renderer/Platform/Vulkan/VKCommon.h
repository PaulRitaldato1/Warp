#pragma once

#ifdef WARP_BUILD_VK

// No platform surface defines: GLFW creates the surface, so nothing here needs
// Win32 or Xlib headers.
#include <vulkan/vulkan.h>
#include <Common/CommonTypes.h>
#include <Debugging/Assert.h>
#include <Debugging/Logging.h>

// ---------------------------------------------------------------------------
// VK_CHECK — log + assert on non-success result
// ---------------------------------------------------------------------------

inline void VkCheck(VkResult result, const char* msg)
{
	if (result != VK_SUCCESS)
	{
		LOG_ERROR("{}  (VkResult={})", msg, static_cast<int>(result));
		FATAL_ASSERT(false, msg);
	}
}

#define VK_CHECK(expr, msg) VkCheck((expr), (msg))

#endif // WARP_BUILD_VK
