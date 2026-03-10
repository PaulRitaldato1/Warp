#pragma once

#ifdef WARP_BUILD_VK

#include <Rendering/Renderer/Platform/Vulkan/VKCommon.h>
#include <Rendering/Renderer/Texture.h>
#include <Rendering/Renderer/ResourceState.h>
#include <Rendering/Renderer/Pipeline.h>

// ---------------------------------------------------------------------------
// Texture format → VkFormat
// ---------------------------------------------------------------------------

inline VkFormat ToVkFormat(TextureFormat fmt)
{
	switch (fmt)
	{
		case TextureFormat::RGBA8:           return VK_FORMAT_R8G8B8A8_UNORM;
		case TextureFormat::RGBA8_SRGB:      return VK_FORMAT_R8G8B8A8_SRGB;
		case TextureFormat::BGRA8:           return VK_FORMAT_B8G8R8A8_UNORM;
		case TextureFormat::RG8:             return VK_FORMAT_R8G8_UNORM;
		case TextureFormat::R8:              return VK_FORMAT_R8_UNORM;
		case TextureFormat::RGBA16F:         return VK_FORMAT_R16G16B16A16_SFLOAT;
		case TextureFormat::RGBA32F:         return VK_FORMAT_R32G32B32A32_SFLOAT;
		case TextureFormat::RGB32F:          return VK_FORMAT_R32G32B32_SFLOAT;
		case TextureFormat::RG32F:           return VK_FORMAT_R32G32_SFLOAT;
		case TextureFormat::R32F:            return VK_FORMAT_R32_SFLOAT;
		case TextureFormat::BC1:             return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
		case TextureFormat::BC3:             return VK_FORMAT_BC3_UNORM_BLOCK;
		case TextureFormat::BC4:             return VK_FORMAT_BC4_UNORM_BLOCK;
		case TextureFormat::BC5:             return VK_FORMAT_BC5_UNORM_BLOCK;
		case TextureFormat::BC7:             return VK_FORMAT_BC7_UNORM_BLOCK;
		case TextureFormat::Depth24Stencil8: return VK_FORMAT_D24_UNORM_S8_UINT;
		case TextureFormat::Depth32F:        return VK_FORMAT_D32_SFLOAT;
		default:                             return VK_FORMAT_UNDEFINED;
	}
}

// ---------------------------------------------------------------------------
// Resource state → VkImageLayout + synchronization2 access/stage masks
// ---------------------------------------------------------------------------

struct VkResourceStateInfo
{
	VkImageLayout         layout;
	VkAccessFlags2        access;
	VkPipelineStageFlags2 stages;
};

inline VkResourceStateInfo ToVkResourceStateInfo(ResourceState state)
{
	switch (state)
	{
		case ResourceState::Undefined:
			return { VK_IMAGE_LAYOUT_UNDEFINED, 0, VK_PIPELINE_STAGE_2_NONE };

		case ResourceState::ShaderResource:
			return {
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
				VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT
			};

		case ResourceState::UnorderedAccess:
			return {
				VK_IMAGE_LAYOUT_GENERAL,
				VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
				VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
			};

		case ResourceState::RenderTarget:
			return {
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
			};

		case ResourceState::DepthWrite:
			return {
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
				VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT
			};

		case ResourceState::DepthRead:
			return {
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
				VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
				VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT
			};

		case ResourceState::CopySource:
			return {
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				VK_ACCESS_2_TRANSFER_READ_BIT,
				VK_PIPELINE_STAGE_2_TRANSFER_BIT
			};

		case ResourceState::CopyDest:
			return {
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_ACCESS_2_TRANSFER_WRITE_BIT,
				VK_PIPELINE_STAGE_2_TRANSFER_BIT
			};

		case ResourceState::Present:
			return {
				VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				0,
				VK_PIPELINE_STAGE_2_NONE
			};

		// Buffer states — not meaningful as image layouts; return a sensible default.
		case ResourceState::VertexBuffer:
		case ResourceState::IndexBuffer:
		case ResourceState::ConstantBuffer:
		default:
			return { VK_IMAGE_LAYOUT_UNDEFINED, 0, VK_PIPELINE_STAGE_2_NONE };
	}
}

// Buffer access/stage masks (no layout for buffers)
inline VkAccessFlags2 ToVkBufferAccess(ResourceState state)
{
	switch (state)
	{
		case ResourceState::VertexBuffer:   return VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
		case ResourceState::IndexBuffer:    return VK_ACCESS_2_INDEX_READ_BIT;
		case ResourceState::ConstantBuffer: return VK_ACCESS_2_UNIFORM_READ_BIT;
		case ResourceState::CopySource:     return VK_ACCESS_2_TRANSFER_READ_BIT;
		case ResourceState::CopyDest:       return VK_ACCESS_2_TRANSFER_WRITE_BIT;
		default:                            return VK_ACCESS_2_NONE;
	}
}

// Vertex element format → byte size (for stride / offset computation)
inline u32 FormatByteSize(TextureFormat fmt)
{
	switch (fmt)
	{
		case TextureFormat::RGBA8:
		case TextureFormat::RGBA8_SRGB:
		case TextureFormat::BGRA8:    return 4;
		case TextureFormat::RG8:      return 2;
		case TextureFormat::R8:       return 1;
		case TextureFormat::RGBA16F:  return 8;
		case TextureFormat::RGBA32F:  return 16;
		case TextureFormat::RGB32F:   return 12;
		case TextureFormat::RG32F:    return 8;
		case TextureFormat::R32F:     return 4;
		default:                      return 0;
	}
}

// Primitive topology → VkPrimitiveTopology
inline VkPrimitiveTopology ToVkTopology(PrimitiveTopology topo)
{
	switch (topo)
	{
		case PrimitiveTopology::PointList:     return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		case PrimitiveTopology::LineList:      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
		case PrimitiveTopology::LineStrip:     return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
		case PrimitiveTopology::TriangleList:  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		case PrimitiveTopology::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		default:                               return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	}
}

#endif // WARP_BUILD_VK
