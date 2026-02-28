#pragma once

#include <Common/CommonTypes.h>

// Platform-agnostic CPU descriptor handle.
//   D3D12  — ptr maps 1:1 to D3D12_CPU_DESCRIPTOR_HANDLE.ptr (SIZE_T); width/height unused
//   Vulkan — ptr holds a VkImageView cast to u64; width/height carry the image extent for
//             vkCmdBeginRendering render-area and vkCmdClearAttachments rect
struct DescriptorHandle
{
	u64  ptr    = 0;
	u32  width  = 0;   // image width  — populated by Vulkan swap-chain / texture
	u32  height = 0;   // image height — populated by Vulkan swap-chain / texture
	bool IsValid() const { return ptr != 0; }
};
