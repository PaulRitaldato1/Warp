#ifdef WARP_BUILD_VK

#include <Rendering/Renderer/Platform/Vulkan/VKCommandList.h>
#include <Rendering/Renderer/Platform/Vulkan/VKPipeline.h>
#include <Rendering/Renderer/Platform/Vulkan/VKBuffer.h>
#include <Rendering/Renderer/Platform/Vulkan/VKTexture.h>
#include <Rendering/Renderer/Platform/Vulkan/VKTranslate.h>
#include <Debugging/Assert.h>
#include <Debugging/Logging.h>

VKCommandList::~VKCommandList()
{
	for (VkCommandPool pool : m_pools)
	{
		if (pool != VK_NULL_HANDLE)
		{
			vkDestroyCommandPool(m_device, pool, nullptr);
		}
	}
	m_pools.clear();
}

void VKCommandList::InitializeWithDevice(VkDevice device, u32 familyIndex, u32 framesInFlight,
                                          VkSampler defaultSampler)
{
	DYNAMIC_ASSERT(device,          "VKCommandList: device is null");
	DYNAMIC_ASSERT(framesInFlight > 0, "VKCommandList: framesInFlight must be > 0");

	m_device         = device;
	m_defaultSampler = defaultSampler;
	m_pushDescriptorFn = reinterpret_cast<PFN_vkCmdPushDescriptorSetKHR>(
		vkGetDeviceProcAddr(device, "vkCmdPushDescriptorSetKHR"));
	if (!m_pushDescriptorFn)
	{
		LOG_WARNING("VKCommandList: vkCmdPushDescriptorSetKHR not available — texture binding disabled");
	}
	m_pools.resize(framesInFlight, VK_NULL_HANDLE);

	for (u32 i = 0; i < framesInFlight; ++i)
	{
		VkCommandPoolCreateInfo poolInfo = {};
		poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = familyIndex;
		poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &m_pools[i]),
		         "VKCommandList: vkCreateCommandPool failed");
	}

	// Allocate the command buffer from pool[0] initially; it gets re-allocated
	// if needed, or simply re-recorded from the same underlying buffer.
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool        = m_pools[0];
	allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &m_cmdBuf),
	         "VKCommandList: vkAllocateCommandBuffers failed");

	LOG_DEBUG("VKCommandList initialized");
}

// ---------------------------------------------------------------------------
// Frame lifecycle
// ---------------------------------------------------------------------------

void VKCommandList::Begin(u32 frameIndex)
{
	DYNAMIC_ASSERT(frameIndex < m_pools.size(), "VKCommandList::Begin: frameIndex out of range");

	// Reset the pool for this frame slot (frees all allocations in it).
	vkResetCommandPool(m_device, m_pools[frameIndex], 0);

	// Re-allocate the command buffer from the newly reset pool.
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool        = m_pools[frameIndex];
	allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	VK_CHECK(vkAllocateCommandBuffers(m_device, &allocInfo, &m_cmdBuf),
	         "VKCommandList::Begin: vkAllocateCommandBuffers failed");

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_CHECK(vkBeginCommandBuffer(m_cmdBuf, &beginInfo),
	         "VKCommandList::Begin: vkBeginCommandBuffer failed");

	m_inRenderPass = false;
}

void VKCommandList::End()
{
	if (m_inRenderPass)
	{
		vkCmdEndRendering(m_cmdBuf);
		m_inRenderPass = false;
	}

	VK_CHECK(vkEndCommandBuffer(m_cmdBuf),
	         "VKCommandList::End: vkEndCommandBuffer failed");
}

// ---------------------------------------------------------------------------
// GPU debug markers (VK_EXT_debug_utils — no-op if not loaded)
// ---------------------------------------------------------------------------

void VKCommandList::BeginEvent(std::string_view name)
{
	(void)name;
	// TODO: vkCmdBeginDebugUtilsLabelEXT when the extension is loaded at runtime
}

void VKCommandList::EndEvent()
{
	// TODO: vkCmdEndDebugUtilsLabelEXT
}

void VKCommandList::SetMarker(std::string_view name)
{
	(void)name;
	// TODO: vkCmdInsertDebugUtilsLabelEXT
}

// ---------------------------------------------------------------------------
// Pipeline state
// ---------------------------------------------------------------------------

void VKCommandList::SetPipelineState(PipelineState* state)
{
	DYNAMIC_ASSERT(state, "VKCommandList::SetPipelineState: state is null");
	VKPipeline* vkPipeline = static_cast<VKPipeline*>(state);
	m_currentLayout = vkPipeline->GetNativeLayout();
	vkCmdBindPipeline(m_cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipeline->GetNativePipeline());
}

void VKCommandList::SetComputePipelineState(ComputePipelineState* state)
{
	DYNAMIC_ASSERT(state, "VKCommandList::SetComputePipelineState: state is null");
	VKComputePipeline* vkPipeline = static_cast<VKComputePipeline*>(state);
	vkCmdBindPipeline(m_cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, vkPipeline->GetNativePipeline());
}

// ---------------------------------------------------------------------------
// Input assembly
// ---------------------------------------------------------------------------

void VKCommandList::SetVertexBuffer(Buffer* vb)
{
	DYNAMIC_ASSERT(vb, "VKCommandList::SetVertexBuffer: vb is null");
	VKBuffer* vkBuf = static_cast<VKBuffer*>(vb);
	VkBuffer  buf   = vkBuf->GetNativeBuffer();
	VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(m_cmdBuf, 0, 1, &buf, &offset);
}

void VKCommandList::SetIndexBuffer(Buffer* ib)
{
	DYNAMIC_ASSERT(ib, "VKCommandList::SetIndexBuffer: ib is null");
	VKBuffer* vkBuf = static_cast<VKBuffer*>(ib);
	vkCmdBindIndexBuffer(m_cmdBuf, vkBuf->GetNativeBuffer(), 0, VK_INDEX_TYPE_UINT32);
}

void VKCommandList::SetPrimitiveTopology(PrimitiveTopology /*topo*/)
{
	// Topology is baked into the Vulkan PSO — no dynamic topology for now.
}

// ---------------------------------------------------------------------------
// Viewport & scissor
// ---------------------------------------------------------------------------

void VKCommandList::SetViewport(f32 x, f32 y, f32 width, f32 height,
                                f32 minDepth, f32 maxDepth)
{
	// Vulkan clip space has Y increasing downward; D3D12 has Y increasing upward.
	// VK_KHR_maintenance1 (core since Vulkan 1.1) allows a negative viewport height
	// to flip Y, making both APIs use the same NDC convention without shader changes.
	VkViewport vp = {};
	vp.x        = x;
	vp.y        = y + height; // origin at bottom-left
	vp.width    = width;
	vp.height   = -height;   // negative flips Y
	vp.minDepth = minDepth;
	vp.maxDepth = maxDepth;
	vkCmdSetViewport(m_cmdBuf, 0, 1, &vp);
}

void VKCommandList::SetScissorRect(u32 left, u32 top, u32 right, u32 bottom)
{
	VkRect2D rect = {};
	rect.offset = { static_cast<int32_t>(left), static_cast<int32_t>(top) };
	rect.extent = { right - left, bottom - top };
	vkCmdSetScissor(m_cmdBuf, 0, 1, &rect);
}

// ---------------------------------------------------------------------------
// Render output binding — dynamic rendering
// ---------------------------------------------------------------------------

void VKCommandList::SetRenderTargets(u32 rtvCount, const DescriptorHandle* rtvs,
                                     DescriptorHandle dsv)
{
	DYNAMIC_ASSERT(rtvCount <= k_maxRTVs, "VKCommandList: max 8 RTVs");

	// End any previous render pass before starting a new one.
	if (m_inRenderPass)
	{
		vkCmdEndRendering(m_cmdBuf);
		m_inRenderPass = false;
	}

	// Store the current render targets for ClearRenderTarget / ClearDepthStencil.
	for (u32 i = 0; i < rtvCount; ++i)
	{
		m_currentRTVs[i] = rtvs[i];
	}
	m_rtvCount   = rtvCount;
	m_currentDSV = dsv;

	// Derive the render area from the first attachment's stored extent.
	u32 areaWidth  = (rtvCount > 0) ? rtvs[0].width  : dsv.width;
	u32 areaHeight = (rtvCount > 0) ? rtvs[0].height : dsv.height;

	// Build colour attachment infos (load = LOAD so explicit clears work via vkCmdClearAttachments).
	VkRenderingAttachmentInfoKHR colourAttachments[k_maxRTVs] = {};
	for (u32 i = 0; i < rtvCount; ++i)
	{
		colourAttachments[i].sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
		colourAttachments[i].imageView   = reinterpret_cast<VkImageView>(static_cast<uintptr_t>(rtvs[i].ptr));
		colourAttachments[i].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		colourAttachments[i].loadOp      = VK_ATTACHMENT_LOAD_OP_LOAD;
		colourAttachments[i].storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
	}

	// Depth attachment info.
	VkRenderingAttachmentInfoKHR depthAttachment = {};
	if (dsv.IsValid())
	{
		depthAttachment.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
		depthAttachment.imageView   = reinterpret_cast<VkImageView>(static_cast<uintptr_t>(dsv.ptr));
		depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		depthAttachment.loadOp      = VK_ATTACHMENT_LOAD_OP_LOAD;
		depthAttachment.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
	}

	VkRenderingInfoKHR renderingInfo = {};
	renderingInfo.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
	renderingInfo.renderArea.offset    = { 0, 0 };
	renderingInfo.renderArea.extent    = { areaWidth, areaHeight };
	renderingInfo.layerCount           = 1;
	renderingInfo.colorAttachmentCount = rtvCount;
	renderingInfo.pColorAttachments    = colourAttachments;
	renderingInfo.pDepthAttachment     = dsv.IsValid() ? &depthAttachment : nullptr;

	vkCmdBeginRendering(m_cmdBuf, &renderingInfo);
	m_inRenderPass = true;
}

void VKCommandList::ClearRenderTarget(DescriptorHandle rtv, f32 r, f32 g, f32 b, f32 a)
{
	// Find attachment index by matching the stored RTV handle.
	u32 attachIndex = 0;
	for (u32 i = 0; i < m_rtvCount; ++i)
	{
		if (m_currentRTVs[i].ptr == rtv.ptr)
		{
			attachIndex = i;
			break;
		}
	}

	VkClearAttachment clearAtt = {};
	clearAtt.aspectMask                  = VK_IMAGE_ASPECT_COLOR_BIT;
	clearAtt.colorAttachment             = attachIndex;
	clearAtt.clearValue.color.float32[0] = r;
	clearAtt.clearValue.color.float32[1] = g;
	clearAtt.clearValue.color.float32[2] = b;
	clearAtt.clearValue.color.float32[3] = a;

	VkClearRect clearRect = {};
	clearRect.rect            = { {0, 0}, { rtv.width, rtv.height } };
	clearRect.baseArrayLayer  = 0;
	clearRect.layerCount      = 1;

	vkCmdClearAttachments(m_cmdBuf, 1, &clearAtt, 1, &clearRect);
}

void VKCommandList::ClearDepthStencil(DescriptorHandle dsv, f32 depth, u8 stencil)
{
	VkClearAttachment clearAtt = {};
	clearAtt.aspectMask                     = VK_IMAGE_ASPECT_DEPTH_BIT;
	clearAtt.clearValue.depthStencil.depth  = depth;
	clearAtt.clearValue.depthStencil.stencil= stencil;

	VkClearRect clearRect = {};
	clearRect.rect           = { {0, 0}, { dsv.width, dsv.height } };
	clearRect.baseArrayLayer = 0;
	clearRect.layerCount     = 1;

	vkCmdClearAttachments(m_cmdBuf, 1, &clearAtt, 1, &clearRect);
}

void VKCommandList::EndCurrentRenderPass()
{
	if (m_inRenderPass)
	{
		vkCmdEndRendering(m_cmdBuf);
		m_inRenderPass = false;
	}
}

// ---------------------------------------------------------------------------
// Copy / transfer
// ---------------------------------------------------------------------------

void VKCommandList::CopyBuffer(Buffer* src, Buffer* dst,
                                u64 srcOffset, u64 dstOffset, u64 size)
{
	DYNAMIC_ASSERT(src, "VKCommandList::CopyBuffer: src is null");
	DYNAMIC_ASSERT(dst, "VKCommandList::CopyBuffer: dst is null");

	VKBuffer* vkSrc = static_cast<VKBuffer*>(src);
	VKBuffer* vkDst = static_cast<VKBuffer*>(dst);

	VkBufferCopy region = {};
	region.srcOffset = srcOffset;
	region.dstOffset = dstOffset;
	region.size      = size;

	vkCmdCopyBuffer(m_cmdBuf, vkSrc->GetNativeBuffer(), vkDst->GetNativeBuffer(), 1, &region);
}

void VKCommandList::CopyBufferToTexture(Buffer* src, u64 srcOffset, u32 srcRowPitch,
                                         Texture* dst, u32 mipLevel, u32 arraySlice)
{
	DYNAMIC_ASSERT(src, "VKCommandList::CopyBufferToTexture: src is null");
	DYNAMIC_ASSERT(dst, "VKCommandList::CopyBufferToTexture: dst is null");

	VKBuffer*  vkSrc = static_cast<VKBuffer*>(src);
	VKTexture* vkDst = static_cast<VKTexture*>(dst);

	const u32 fullWidth  = vkDst->GetWidth();
	const u32 fullHeight = vkDst->GetHeight();
	const u32 mipWidth   = (fullWidth  >> mipLevel) > 0 ? (fullWidth  >> mipLevel) : 1;
	const u32 mipHeight  = (fullHeight >> mipLevel) > 0 ? (fullHeight >> mipLevel) : 1;

	// Vulkan bufferRowLength is in texels. Convert from the byte srcRowPitch.
	// For BC formats: (srcRowPitch / blockSizeBytes) * 4 texels per block width.
	// For uncompressed: srcRowPitch / bytesPerTexel.
	// If 0, Vulkan treats the data as tightly packed (bufferRowLength == mipWidth).
	u32 bufferRowLength = 0;
	const TextureFormat fmt = vkDst->GetFormat();
	switch (fmt)
	{
		case TextureFormat::BC1: case TextureFormat::BC4: bufferRowLength = (srcRowPitch / 8)  * 4; break;
		case TextureFormat::BC3: case TextureFormat::BC5:
		case TextureFormat::BC7:                          bufferRowLength = (srcRowPitch / 16) * 4; break;
		default:
		{
			// Uncompressed: compute texel count from byte pitch.
			// BytesPerPixel for the common formats; 0 means unknown → fall back to tight packing.
			static constexpr u32 k_bpp[] = { 0,4,4,4,2,1,8,16,12,8,4,0,0,0,0,0,0,0 };
			const u32 idx = static_cast<u32>(fmt);
			const u32 bpp = idx < sizeof(k_bpp) / sizeof(k_bpp[0]) ? k_bpp[idx] : 0;
			bufferRowLength = (bpp > 0) ? (srcRowPitch / bpp) : 0;
			break;
		}
	}

	VkBufferImageCopy region = {};
	region.bufferOffset      = srcOffset;
	region.bufferRowLength   = bufferRowLength;
	region.bufferImageHeight = 0; // tightly packed in height
	region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel       = mipLevel;
	region.imageSubresource.baseArrayLayer = arraySlice;
	region.imageSubresource.layerCount     = 1;
	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = { mipWidth, mipHeight, 1 };

	vkCmdCopyBufferToImage(m_cmdBuf,
	                       vkSrc->GetNativeBuffer(),
	                       vkDst->GetNativeImage(),
	                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	                       1, &region);
}

// ---------------------------------------------------------------------------
// Resource transitions (synchronization2)
// ---------------------------------------------------------------------------

void VKCommandList::TransitionTexture(Texture* texture, ResourceState newState)
{
	DYNAMIC_ASSERT(texture, "VKCommandList::TransitionTexture: texture is null");
	VKTexture* vkTex = static_cast<VKTexture*>(texture);

	VkResourceStateInfo srcInfo = ToVkResourceStateInfo(ResourceState::Undefined);
	VkResourceStateInfo dstInfo = ToVkResourceStateInfo(newState);

	// Determine the correct source info from the current layout.
	VkImageLayout currentLayout = vkTex->GetCurrentLayout();
	for (int s = 0; s <= static_cast<int>(ResourceState::Present); ++s)
	{
		VkResourceStateInfo candidate = ToVkResourceStateInfo(static_cast<ResourceState>(s));
		if (candidate.layout == currentLayout)
		{
			srcInfo = candidate;
			break;
		}
	}

	if (currentLayout == dstInfo.layout)
	{
		return; // Already in the target layout.
	}

	const bool isDepth = (vkTex->GetFormat() == TextureFormat::Depth32F ||
	                      vkTex->GetFormat() == TextureFormat::Depth24Stencil8);
	VkImageAspectFlags aspect = isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

	VkImageMemoryBarrier2 barrier = {};
	barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	barrier.srcStageMask        = srcInfo.stages  ? srcInfo.stages  : VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
	barrier.srcAccessMask       = srcInfo.access;
	barrier.dstStageMask        = dstInfo.stages  ? dstInfo.stages  : VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
	barrier.dstAccessMask       = dstInfo.access;
	barrier.oldLayout           = currentLayout;
	barrier.newLayout           = dstInfo.layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image               = vkTex->GetNativeImage();
	barrier.subresourceRange    = { aspect, 0, vkTex->GetMipLevels(), 0, 1 };

	VkDependencyInfo depInfo = {};
	depInfo.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depInfo.imageMemoryBarrierCount = 1;
	depInfo.pImageMemoryBarriers    = &barrier;

	vkCmdPipelineBarrier2(m_cmdBuf, &depInfo);
	vkTex->SetCurrentLayout(dstInfo.layout);
}

void VKCommandList::TransitionBuffer(Buffer* buffer, ResourceState newState)
{
	DYNAMIC_ASSERT(buffer, "VKCommandList::TransitionBuffer: buffer is null");
	VKBuffer* vkBuf = static_cast<VKBuffer*>(buffer);

	// Staging buffers are always HOST_VISIBLE — skip barriers.
	if (vkBuf->IsStagingBuffer())
	{
		return;
	}

	VkAccessFlags2 srcAccess = vkBuf->GetCurrentAccess();
	VkAccessFlags2 dstAccess = ToVkBufferAccess(newState);

	if (srcAccess == dstAccess)
	{
		return;
	}

	VkBufferMemoryBarrier2 barrier = {};
	barrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	barrier.srcStageMask        = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	barrier.srcAccessMask       = srcAccess;
	barrier.dstStageMask        = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	barrier.dstAccessMask       = dstAccess;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.buffer              = vkBuf->GetNativeBuffer();
	barrier.offset              = 0;
	barrier.size                = VK_WHOLE_SIZE;

	VkDependencyInfo depInfo = {};
	depInfo.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depInfo.bufferMemoryBarrierCount = 1;
	depInfo.pBufferMemoryBarriers    = &barrier;

	vkCmdPipelineBarrier2(m_cmdBuf, &depInfo);
	vkBuf->SetCurrentAccess(dstAccess);
}

// ---------------------------------------------------------------------------
// Resource binding stubs — push-constant / descriptor system is future work
// ---------------------------------------------------------------------------

void VKCommandList::SetConstantBuffer(u32 /*rootIndex*/, Buffer* /*buffer*/)
{
	// TODO: vkCmdPushConstants once a proper pipeline layout is in place
}

void VKCommandList::PushConstants(u32 size, const void* data)
{
	DYNAMIC_ASSERT(m_currentLayout != VK_NULL_HANDLE, "VKCommandList::PushConstants: no pipeline bound — call SetPipelineState first");
	DYNAMIC_ASSERT(data, "VKCommandList::PushConstants: data is null");
	vkCmdPushConstants(m_cmdBuf, m_currentLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, size, data);
}

void VKCommandList::SetShaderResource(u32 rootIndex, Texture* texture)
{
	if (!m_pushDescriptorFn || !texture || m_currentLayout == VK_NULL_HANDLE)
		return;

	VKTexture* vkTex = static_cast<VKTexture*>(texture);

	VkDescriptorImageInfo imageInfo = {};
	imageInfo.sampler     = m_defaultSampler;
	imageInfo.imageView   = vkTex->GetNativeView();
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkWriteDescriptorSet write = {};
	write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstBinding      = rootIndex;
	write.dstArrayElement = 0;
	write.descriptorCount = 1;
	write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo      = &imageInfo;

	m_pushDescriptorFn(m_cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_currentLayout, 0, 1, &write);
}

void VKCommandList::SetShaderResources(u32 /*rootIndex*/, const Vector<Texture*>& textures)
{
	if (!m_pushDescriptorFn || textures.empty() || m_currentLayout == VK_NULL_HANDLE)
		return;

	Vector<VkDescriptorImageInfo> imageInfos;
	Vector<VkWriteDescriptorSet>  writes;
	imageInfos.reserve(textures.size());
	writes.reserve(textures.size());

	for (u32 i = 0; i < static_cast<u32>(textures.size()); ++i)
	{
		if (!textures[i])
			continue;

		VKTexture* vkTex = static_cast<VKTexture*>(textures[i]);

		VkDescriptorImageInfo& info = imageInfos.emplace_back();
		info.sampler     = m_defaultSampler;
		info.imageView   = vkTex->GetNativeView();
		info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet& write = writes.emplace_back();
		write                 = {};
		write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstBinding      = i;
		write.dstArrayElement = 0;
		write.descriptorCount = 1;
		write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write.pImageInfo      = &imageInfos.back();
	}

	if (!writes.empty())
	{
		m_pushDescriptorFn(m_cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_currentLayout, 0,
		                   static_cast<u32>(writes.size()), writes.data());
	}
}

// ---------------------------------------------------------------------------
// Draw / dispatch
// ---------------------------------------------------------------------------

void VKCommandList::Draw(u32 vertexCount, u32 instanceCount,
                         u32 firstVertex, u32 firstInstance)
{
	vkCmdDraw(m_cmdBuf, vertexCount, instanceCount, firstVertex, firstInstance);
}

void VKCommandList::DrawIndexed(u32 indexCount, u32 instanceCount,
                                u32 firstIndex, u32 baseVertex, u32 firstInstance)
{
	vkCmdDrawIndexed(m_cmdBuf, indexCount, instanceCount, firstIndex,
	                 static_cast<int32_t>(baseVertex), firstInstance);
}

void VKCommandList::Dispatch(u32 x, u32 y, u32 z)
{
	vkCmdDispatch(m_cmdBuf, x, y, z);
}

#endif // WARP_BUILD_VK
