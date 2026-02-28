#ifdef WARP_BUILD_VK

#include <Rendering/Renderer/Platform/Vulkan/VKTexture.h>
#include <Rendering/Renderer/Platform/Vulkan/VKTranslate.h>
#include <Debugging/Assert.h>

VKTexture::~VKTexture()
{
	Cleanup();
}

void VKTexture::InitializeWithAllocator(VmaAllocator allocator, VkDevice device)
{
	DYNAMIC_ASSERT(allocator, "VKTexture: allocator is null");
	DYNAMIC_ASSERT(device,    "VKTexture: device is null");
	m_allocator = allocator;
	m_device    = device;
}

static VkImageType ToVkImageType(TextureType type)
{
	switch (type)
	{
		case TextureType::Texture2D: return VK_IMAGE_TYPE_2D;
		case TextureType::Texture3D: return VK_IMAGE_TYPE_3D;
		case TextureType::CubeMap:   return VK_IMAGE_TYPE_2D; // cube faces are 2-D layers
		default:                     return VK_IMAGE_TYPE_2D;
	}
}

static VkImageViewType ToVkImageViewType(TextureType type)
{
	switch (type)
	{
		case TextureType::Texture2D: return VK_IMAGE_VIEW_TYPE_2D;
		case TextureType::Texture3D: return VK_IMAGE_VIEW_TYPE_3D;
		case TextureType::CubeMap:   return VK_IMAGE_VIEW_TYPE_CUBE;
		default:                     return VK_IMAGE_VIEW_TYPE_2D;
	}
}

void VKTexture::Init(const TextureDesc& desc)
{
	m_width     = desc.width;
	m_height    = desc.height;
	m_depth     = desc.depth;
	m_mipLevels = desc.mipLevels;
	m_format    = desc.format;
	m_usage     = desc.usage;
	m_type      = desc.type;

	const bool isDepth = (desc.format == TextureFormat::Depth32F ||
	                      desc.format == TextureFormat::Depth24Stencil8);

	const u32 arrayLayers = (desc.type == TextureType::CubeMap)
		? 6u
		: desc.arrayLayers;

	// -------------------------------------------------------------------------
	// VkImage
	// -------------------------------------------------------------------------

	VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
	                               VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	switch (desc.usage)
	{
		case TextureUsage::Sampled:
			usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
			break;
		case TextureUsage::RenderTarget:
			usageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			break;
		case TextureUsage::DepthStencil:
			usageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			break;
		case TextureUsage::Storage:
			usageFlags |= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			break;
	}

	VkImageCreateFlags createFlags = 0;
	if (desc.type == TextureType::CubeMap)
	{
		createFlags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	}

	VkImageCreateInfo imageInfo = {};
	imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.flags         = createFlags;
	imageInfo.imageType     = ToVkImageType(desc.type);
	imageInfo.format        = ToVkFormat(desc.format);
	imageInfo.extent        = { desc.width, desc.height, (desc.type == TextureType::Texture3D) ? desc.depth : 1u };
	imageInfo.mipLevels     = desc.mipLevels;
	imageInfo.arrayLayers   = arrayLayers;
	imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage         = usageFlags;
	imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	VK_CHECK(vmaCreateImage(m_allocator, &imageInfo, &allocInfo,
	                        &m_image, &m_allocation, nullptr),
	         "VKTexture: vmaCreateImage failed");

	m_currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	// -------------------------------------------------------------------------
	// VkImageView
	// -------------------------------------------------------------------------

	VkImageAspectFlags aspect = isDepth
		? VK_IMAGE_ASPECT_DEPTH_BIT
		: VK_IMAGE_ASPECT_COLOR_BIT;

	VkImageViewCreateInfo viewInfo = {};
	viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image                           = m_image;
	viewInfo.viewType                        = ToVkImageViewType(desc.type);
	viewInfo.format                          = ToVkFormat(desc.format);
	viewInfo.subresourceRange.aspectMask     = aspect;
	viewInfo.subresourceRange.baseMipLevel   = 0;
	viewInfo.subresourceRange.levelCount     = desc.mipLevels;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount     = arrayLayers;

	VK_CHECK(vkCreateImageView(m_device, &viewInfo, nullptr, &m_view),
	         "VKTexture: vkCreateImageView failed");
}

void VKTexture::Cleanup()
{
	if (m_view != VK_NULL_HANDLE)
	{
		vkDestroyImageView(m_device, m_view, nullptr);
		m_view = VK_NULL_HANDLE;
	}
	if (m_image != VK_NULL_HANDLE)
	{
		vmaDestroyImage(m_allocator, m_image, m_allocation);
		m_image      = VK_NULL_HANDLE;
		m_allocation = VK_NULL_HANDLE;
	}
}

DescriptorHandle VKTexture::GetDescriptorHandle() const
{
	DescriptorHandle h;
	h.ptr    = static_cast<u64>(reinterpret_cast<uintptr_t>(m_view));
	h.width  = m_width;
	h.height = m_height;
	return h;
}

#endif // WARP_BUILD_VK
