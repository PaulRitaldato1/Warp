#pragma once

#include <Common/CommonTypes.h>
#include <Rendering/Renderer/Pipeline.h>       // PrimitiveTopology, PipelineState, ComputePipelineState
#include <Rendering/Renderer/ResourceState.h>
#include <Rendering/Renderer/DescriptorHandle.h>

class Buffer;
class Texture;

class CommandList
{
public:
	virtual ~CommandList() = default;

	// frameIndex selects which internal allocator slot to reset.
	virtual void Begin(u32 frameIndex) = 0;
	virtual void End()                 = 0;

	// GPU debug markers — no-op by default; override in backends that support PIX/RenderDoc.
	virtual void BeginEvent(std::string_view name) {}
	virtual void EndEvent()                        {}
	virtual void SetMarker(std::string_view name)  {}

	// ---------------------------------------------------------------------------
	// State binding
	// ---------------------------------------------------------------------------

	// Sets the graphics PSO (and root signature on D3D12).
	virtual void SetPipelineState(PipelineState* state) = 0;

	// Sets the compute PSO (and compute root signature on D3D12).
	virtual void SetComputePipelineState(ComputePipelineState* state) = 0;

	// Input assembly
	virtual void SetVertexBuffer(Buffer* vertexBuffer)         = 0;
	virtual void SetIndexBuffer(Buffer* indexBuffer)           = 0;
	virtual void SetPrimitiveTopology(PrimitiveTopology topo)  = 0;

	// Viewport and scissor (pixel coordinates)
	virtual void SetViewport(f32 x, f32 y, f32 width, f32 height,
	                         f32 minDepth = 0.f, f32 maxDepth = 1.f) = 0;
	virtual void SetScissorRect(u32 left, u32 top, u32 right, u32 bottom) = 0;

	// ---------------------------------------------------------------------------
	// Render output binding
	// ---------------------------------------------------------------------------

	// Bind up to 8 render-target views and an optional depth/stencil view.
	// Pass an invalid DescriptorHandle for dsv to render without depth.
	virtual void SetRenderTargets(u32 rtvCount, const DescriptorHandle* rtvs,
	                              DescriptorHandle dsv) = 0;

	// Fill a render target with a solid colour.
	virtual void ClearRenderTarget(DescriptorHandle rtv,
	                               f32 r, f32 g, f32 b, f32 a) = 0;

	// Clear depth and/or stencil to the given values.
	virtual void ClearDepthStencil(DescriptorHandle dsv,
	                               f32 depth = 1.f, u8 stencil = 0) = 0;

	// ---------------------------------------------------------------------------
	// Resource transitions
	// ---------------------------------------------------------------------------

	// Inserts a pipeline barrier transitioning a texture from its current state
	// to newState.  The resource tracks its own current state internally.
	virtual void TransitionTexture(Texture* texture, ResourceState newState) = 0;

	// Inserts a pipeline barrier transitioning a buffer to newState.
	// Upload-heap (Dynamic) buffers are always GENERIC_READ and cannot be
	// transitioned — implementations should silently skip barriers for them.
	virtual void TransitionBuffer(Buffer* buffer, ResourceState newState) = 0;

	// Resource binding — rootIndex maps to D3D12 root parameter / Vulkan set+binding.
	// SetConstantBuffer: binds a Buffer as a CBV (b register).
	// SetShaderResource: binds a Texture as an SRV (t register).
	// NOTE: full bindless/descriptor-heap management is a future feature.
	virtual void SetConstantBuffer(u32 rootIndex, Buffer* buffer)   = 0;
	virtual void SetShaderResource(u32 rootIndex, Texture* texture) = 0;

	// ---------------------------------------------------------------------------
	// Draw / dispatch
	// ---------------------------------------------------------------------------

	virtual void Draw(u32 vertexCount, u32 instanceCount = 1,
	                  u32 firstVertex = 0, u32 firstInstance = 0) = 0;

	virtual void DrawIndexed(u32 indexCount, u32 instanceCount = 1,
	                         u32 firstIndex = 0, u32 baseVertex = 0,
	                         u32 firstInstance = 0) = 0;

	virtual void Dispatch(u32 groupCountX, u32 groupCountY, u32 groupCountZ) = 0;
};
