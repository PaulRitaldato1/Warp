#pragma once

#include <Rendering/Renderer/CommandList.h>

#ifdef WARP_WINDOWS

#include <d3d12.h>
#include <Rendering/Renderer/ResourceState.h>
#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12DescriptorHeap.h>

class D3D12CommandList : public CommandList
{
public:
	// framesInFlight creates one command allocator per frame slot.
	void InitializeWithDevice(ID3D12Device* device, u32 framesInFlight,
							  D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT);

	// CommandList interface
	void Begin(u32 frameIndex) override;
	void End() override;

	void BeginEvent(std::string_view name) override;
	void EndEvent() override;
	void SetMarker(std::string_view name) override;

	void SetPipelineState(PipelineState* state) override;
	void SetComputePipelineState(ComputePipelineState* state) override;

	void SetVertexBuffer(Buffer* vertexBuffer) override;
	void SetIndexBuffer(Buffer* indexBuffer) override;
	void SetPrimitiveTopology(PrimitiveTopology topo) override;

	void SetViewport(f32 x, f32 y, f32 width, f32 height,
	                 f32 minDepth = 0.f, f32 maxDepth = 1.f) override;
	void SetScissorRect(u32 left, u32 top, u32 right, u32 bottom) override;

	void SetRenderTargets(u32 rtvCount, const DescriptorHandle* rtvs,
	                      DescriptorHandle dsv)                        override;
	void ClearRenderTarget(DescriptorHandle rtv,
	                       f32 r, f32 g, f32 b, f32 a)                 override;
	void ClearDepthStencil(DescriptorHandle dsv,
	                       f32 depth = 1.f, u8 stencil = 0)            override;

	void SetConstantBuffer(u32 rootIndex, Buffer* buffer)   override;
	void SetShaderResource(u32 rootIndex, Texture* texture) override;

	void TransitionTexture(Texture* texture, ResourceState newState) override;
	void TransitionBuffer(Buffer* buffer, ResourceState newState)    override;

	// D3D12-internal: transition a bare ID3D12Resource (e.g. swap-chain back buffer).
	// Caller is responsible for tracking before/after states.
	void TransitionResource(ID3D12Resource* resource,
	                        D3D12_RESOURCE_STATES before,
	                        D3D12_RESOURCE_STATES after);

	void Draw(u32 vertexCount, u32 instanceCount = 1,
	          u32 firstVertex = 0, u32 firstInstance = 0) override;
	void DrawIndexed(u32 indexCount, u32 instanceCount = 1,
	                 u32 firstIndex = 0, u32 baseVertex = 0,
	                 u32 firstInstance = 0) override;
	void Dispatch(u32 x, u32 y, u32 z) override;

	ID3D12GraphicsCommandList* GetNative() const { return m_list.Get(); }

	// Called once by D3D12Renderer::Init after list creation.
	// The device is used to create inline SRVs in SetShaderResource.
	// The heap is bound via SetDescriptorHeaps at the start of every Begin().
	void SetDevice(ID3D12Device*       device) { m_device  = device; }
	void SetSRVHeap(D3D12DescriptorHeap* heap) { m_srvHeap = heap;   }

private:
	ComRef<ID3D12GraphicsCommandList>      m_list;
	Vector<ComRef<ID3D12CommandAllocator>> m_allocators; // one per frame-in-flight

	// Non-owning — lifetime managed by D3D12Renderer.
	ID3D12Device*      m_device  = nullptr;
	D3D12DescriptorHeap* m_srvHeap = nullptr;
};

#endif
