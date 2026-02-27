#ifdef WARP_BUILD_DX12

#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12CommandList.h>
#include <Debugging/Assert.h>
#include <Debugging/Logging.h>

void D3D12CommandList::InitializeWithDevice(ID3D12Device* device, u32 framesInFlight,
											D3D12_COMMAND_LIST_TYPE type)
{
	DYNAMIC_ASSERT(device,           "D3D12CommandList::InitializeWithDevice: device is null");
	DYNAMIC_ASSERT(framesInFlight > 0, "D3D12CommandList::InitializeWithDevice: framesInFlight must be > 0");

	m_allocators.resize(framesInFlight);
	for (u32 i = 0; i < framesInFlight; ++i)
	{
		ThrowIfFailed(device->CreateCommandAllocator(type, IID_PPV_ARGS(&m_allocators[i])));
	}

	// Create in the closed state; Begin() resets the chosen allocator before recording.
	ThrowIfFailed(device->CreateCommandList(
		0, type, m_allocators[0].Get(), nullptr, IID_PPV_ARGS(&m_list)));
	ThrowIfFailed(m_list->Close());

	LOG_DEBUG("D3D12CommandList initialized");
}

void D3D12CommandList::Begin(u32 frameIndex)
{
	DYNAMIC_ASSERT(frameIndex < m_allocators.size(), "D3D12CommandList::Begin: frameIndex out of range");
	DYNAMIC_ASSERT(m_list, "D3D12CommandList::Begin: list is null");

	ThrowIfFailed(m_allocators[frameIndex]->Reset());
	ThrowIfFailed(m_list->Reset(m_allocators[frameIndex].Get(), nullptr));
}

void D3D12CommandList::End()
{
	DYNAMIC_ASSERT(m_list, "D3D12CommandList::End: list is null");
	ThrowIfFailed(m_list->Close());
}

void D3D12CommandList::BeginEvent(std::string_view /*name*/)
{
	// TODO: wire up WinPixEventRuntime or D3D12 annotation layer
}

void D3D12CommandList::EndEvent()
{
	// TODO: wire up WinPixEventRuntime or D3D12 annotation layer
}

void D3D12CommandList::SetMarker(std::string_view /*name*/)
{
	// TODO: wire up WinPixEventRuntime or D3D12 annotation layer
}

// ---------------------------------------------------------------------------
// Stub implementations — filled in once PSO / resource-binding is done
// ---------------------------------------------------------------------------

void D3D12CommandList::SetPipelineState(URef<PipelineState> /*state*/) {}
void D3D12CommandList::SetVertexBuffer(URef<Buffer> /*vb*/) {}
void D3D12CommandList::SetIndexBuffer(URef<Buffer> /*ib*/) {}
void D3D12CommandList::BindResource(u32 /*slot*/, URef<Resource> /*resource*/) {}
void D3D12CommandList::Draw(u32 /*vc*/, u32 /*ic*/, u32 /*fv*/, u32 /*fi*/) {}
void D3D12CommandList::DrawIndexed(u32 /*ic*/, u32 /*ii*/, u32 /*fi*/, u32 /*fv*/, u32 /*fii*/) {}
void D3D12CommandList::Dispatch(u32 /*x*/, u32 /*y*/, u32 /*z*/) {}

#endif
