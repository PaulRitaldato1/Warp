#pragma once

#ifdef WARP_BUILD_VK

// On Linux with X11, enable the xlib surface extension before including vulkan.h.
// Xlib.h pollutes the global namespace with macros that conflict with our enums
// (None, True, False, Status, Bool) — undef them immediately after inclusion.
#ifdef WARP_LINUX
#   include <X11/Xlib.h>
#   define VK_USE_PLATFORM_XLIB_KHR
    // Clean up X11 macro pollution
#   undef None
#   undef True
#   undef False
#   undef Bool
#   undef Status
#endif

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
		LOG_ERROR(std::string(msg) + "  (VkResult=" + std::to_string(static_cast<int>(result)) + ")");
		FATAL_ASSERT(false, msg);
	}
}

#define VK_CHECK(expr, msg) VkCheck((expr), (msg))

#endif // WARP_BUILD_VK
