#pragma once

#include <Rendering/Renderer/CommandList.h>

#ifdef WARP_WINDOWS

#include <d3d12.h>

class D3D12CommandList : public CommandList
{
public:
	// framesInFlight creates one command allocator per frame slot.
	void InitializeWithDevice(ID3D12Device* device, u32 framesInFlight,
							  D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT);

	// CommandList interface
	void Begin(u32 frameIndex) override;  // resets allocator[frameIndex] + list
	void End() override;                  // calls Close()

	// GPU markers — left as no-ops until PIX/WinPIX is wired up
	void BeginEvent(std::string_view name) override;
	void EndEvent() override;
	void SetMarker(std::string_view name) override;

	// Stub draw-command overrides (filled in when PSO/buffer binding is implemented)
	void SetPipelineState(URef<PipelineState> state) override;
	void SetVertexBuffer(URef<Buffer> vb) override;
	void SetIndexBuffer(URef<Buffer> ib) override;
	void BindResource(u32 slot, URef<Resource> resource) override;
	void Draw(u32 vertexCount, u32 instanceCount = 1, u32 firstVertex = 0,
			  u32 firstInstance = 0) override;
	void DrawIndexed(u32 indexCount, u32 instanceCount = 1, u32 firstIndex = 0,
					 u32 firstVertex = 0, u32 firstInstance = 0) override;
	void Dispatch(u32 x, u32 y, u32 z) override;

	ID3D12GraphicsCommandList* GetNative() const { return m_list.Get(); }

private:
	ComRef<ID3D12GraphicsCommandList>        m_list;
	Vector<ComRef<ID3D12CommandAllocator>>   m_allocators; // one per frame-in-flight
};

#endif
