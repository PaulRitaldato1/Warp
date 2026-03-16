#pragma once

#ifdef WARP_BUILD_VK

#include <Rendering/Renderer/CommandList.h>
#include <Rendering/Renderer/DescriptorHandle.h>
#include <Rendering/Renderer/Platform/Vulkan/VKCommon.h>

// ---------------------------------------------------------------------------
// VKCommandList — wraps a VkCommandBuffer with one VkCommandPool per
// frame-in-flight slot.  Dynamic rendering (Vulkan 1.3) is used in place
// of render passes; state is tracked to support the SetRenderTargets /
// ClearRenderTarget / TransitionToPresent sequence in the base Renderer.
// ---------------------------------------------------------------------------

class VKCommandList : public CommandList
{
public:
	~VKCommandList() override;

	void InitializeWithDevice(VkDevice device, u32 familyIndex, u32 framesInFlight,
	                          VkSampler defaultSampler = VK_NULL_HANDLE);

	// ---------------------------------------------------------------------------
	// Frame lifecycle
	// ---------------------------------------------------------------------------

	void Begin(u32 frameIndex) override;
	void End()                 override;

	// GPU debug markers (uses VK_EXT_debug_utils when available)
	void BeginEvent(std::string_view name) override;
	void EndEvent()                        override;
	void SetMarker(std::string_view name)  override;

	// ---------------------------------------------------------------------------
	// Pipeline state
	// ---------------------------------------------------------------------------

	void SetPipelineState(PipelineState* state)               override;
	void SetComputePipelineState(ComputePipelineState* state) override;

	// ---------------------------------------------------------------------------
	// Input assembly
	// ---------------------------------------------------------------------------

	void SetVertexBuffer(Buffer* vb)                      override;
	void SetIndexBuffer(Buffer* ib)                       override;
	void SetPrimitiveTopology(PrimitiveTopology topo)     override; // no-op — baked into PSO

	// ---------------------------------------------------------------------------
	// Viewport & scissor
	// ---------------------------------------------------------------------------

	void SetViewport(f32 x, f32 y, f32 width, f32 height,
	                 f32 minDepth = 0.f, f32 maxDepth = 1.f) override;
	void SetScissorRect(u32 left, u32 top, u32 right, u32 bottom) override;

	// ---------------------------------------------------------------------------
	// Render output binding  (dynamic rendering)
	// ---------------------------------------------------------------------------

	void SetRenderTargets(u32 rtvCount, const DescriptorHandle* rtvs,
	                      DescriptorHandle dsv)                    override;
	void ClearRenderTarget(DescriptorHandle rtv,
	                       f32 r, f32 g, f32 b, f32 a)            override;
	void ClearDepthStencil(DescriptorHandle dsv,
	                       f32 depth = 1.f, u8 stencil = 0)       override;

	// Called by VKSwapChain::TransitionToPresent before the image barrier.
	void EndCurrentRenderPass();

	// ---------------------------------------------------------------------------
	// Copy / transfer
	// ---------------------------------------------------------------------------

	void CopyBuffer(Buffer* src, Buffer* dst,
	                u64 srcOffset, u64 dstOffset, u64 size) override;
	void CopyBufferToTexture(Buffer* src, u64 srcOffset, u32 srcRowPitch,
	                         Texture* dst, u32 mipLevel, u32 arraySlice) override;

	// ---------------------------------------------------------------------------
	// Resource transitions
	// ---------------------------------------------------------------------------

	void TransitionTexture(Texture* texture, ResourceState newState) override;
	void TransitionBuffer(Buffer* buffer, ResourceState newState)    override;

	// ---------------------------------------------------------------------------
	// Resource binding
	// ---------------------------------------------------------------------------

	void SetConstantBuffer(u32 rootIndex, Buffer* buffer)                              override;
	void SetConstantBufferView(u32 rootIndex, Buffer* buffer, u64 offset, u64 size)    override;
	void SetShaderResource(u32 rootIndex, Texture* texture)                  override;
	void SetShaderResources(u32 rootIndex, const Vector<Texture*>& textures) override;
	void SetShaderResourceBuffer(u32 rootIndex, Buffer* buffer, u64 offset)  override;

	// ---------------------------------------------------------------------------
	// Draw / dispatch
	// ---------------------------------------------------------------------------

	void Draw(u32 vertexCount, u32 instanceCount = 1,
	          u32 firstVertex = 0, u32 firstInstance = 0) override;
	void DrawIndexed(u32 indexCount, u32 instanceCount = 1,
	                 u32 firstIndex = 0, u32 baseVertex = 0,
	                 u32 firstInstance = 0)               override;
	void Dispatch(u32 x, u32 y, u32 z)                   override;

	VkCommandBuffer GetNative() const { return m_cmdBuf; }

private:
	VkDevice               m_device        = VK_NULL_HANDLE;
	Vector<VkCommandPool>  m_pools;          // one per frame slot
	VkCommandBuffer        m_cmdBuf        = VK_NULL_HANDLE;
	VkPipelineLayout       m_currentLayout     = VK_NULL_HANDLE; // cached from last SetPipelineState
	const Vector<u32>*     m_currentBindingMap = nullptr;        // rootIndex -> Vulkan binding index

	// Push descriptor support
	PFN_vkCmdPushDescriptorSetKHR m_pushDescriptorFn = nullptr;
	VkSampler                     m_defaultSampler    = VK_NULL_HANDLE;

	// Current render-pass state (dynamic rendering)
	static constexpr u32 k_maxRTVs = 8;
	DescriptorHandle m_currentRTVs[k_maxRTVs] = {};
	u32              m_rtvCount  = 0;
	DescriptorHandle m_currentDSV;
	bool             m_inRenderPass = false;
};

#endif // WARP_BUILD_VK
