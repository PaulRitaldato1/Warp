#ifdef WARP_BUILD_DX12

#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12CommandList.h>
#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12Pipeline.h>
#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12Buffer.h>
#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12Texture.h>
#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12Translate.h>
#include <Debugging/Assert.h>
#include <Debugging/Logging.h>

void D3D12CommandList::InitializeWithDevice(ID3D12Device* device, u32 framesInFlight,
											D3D12_COMMAND_LIST_TYPE type)
{
	DYNAMIC_ASSERT(device,             "D3D12CommandList::InitializeWithDevice: device is null");
	DYNAMIC_ASSERT(framesInFlight > 0, "D3D12CommandList::InitializeWithDevice: framesInFlight must be > 0");

	m_allocators.resize(framesInFlight);
	for (u32 i = 0; i < framesInFlight; ++i)
	{
		ThrowIfFailed(device->CreateCommandAllocator(type, IID_PPV_ARGS(&m_allocators[i])));
	}

	// Create closed — Begin() resets the chosen allocator before recording.
	ThrowIfFailed(device->CreateCommandList(0, type, m_allocators[0].Get(), nullptr,
											IID_PPV_ARGS(&m_list)));
	ThrowIfFailed(m_list->Close());

	LOG_DEBUG("D3D12CommandList initialized");
}

// ---------------------------------------------------------------------------
// Frame lifecycle
// ---------------------------------------------------------------------------

void D3D12CommandList::Begin(u32 frameIndex)
{
	DYNAMIC_ASSERT(frameIndex < m_allocators.size(), "D3D12CommandList::Begin: frameIndex out of range");
	ThrowIfFailed(m_allocators[frameIndex]->Reset());
	ThrowIfFailed(m_list->Reset(m_allocators[frameIndex].Get(), nullptr));

	// Descriptor heaps must be re-bound after every Reset().
	if (m_srvHeap && m_srvHeap->IsInitialized())
	{
		m_srvHeap->Reset(); // reclaim per-frame dynamic descriptors
		ID3D12DescriptorHeap* heaps[] = { m_srvHeap->GetNative() };
		m_list->SetDescriptorHeaps(1, heaps);
	}
}

void D3D12CommandList::End()
{
	ThrowIfFailed(m_list->Close());
}

// ---------------------------------------------------------------------------
// GPU debug markers
// ---------------------------------------------------------------------------

void D3D12CommandList::BeginEvent(std::string_view /*name*/)  {}
void D3D12CommandList::EndEvent()                             {}
void D3D12CommandList::SetMarker(std::string_view /*name*/)   {}

// ---------------------------------------------------------------------------
// Pipeline state
// ---------------------------------------------------------------------------

void D3D12CommandList::SetPipelineState(PipelineState* state)
{
	DYNAMIC_ASSERT(state, "D3D12CommandList::SetPipelineState: state is null");
	D3D12Pipeline* d3dPipeline = static_cast<D3D12Pipeline*>(state);
	m_list->SetPipelineState(d3dPipeline->GetNativePSO());
	m_list->SetGraphicsRootSignature(d3dPipeline->GetNativeRootSig());
}

void D3D12CommandList::SetComputePipelineState(ComputePipelineState* state)
{
	DYNAMIC_ASSERT(state, "D3D12CommandList::SetComputePipelineState: state is null");
	D3D12ComputePipeline* d3dPipeline = static_cast<D3D12ComputePipeline*>(state);
	m_list->SetPipelineState(d3dPipeline->GetNativePSO());
	m_list->SetComputeRootSignature(d3dPipeline->GetNativeRootSig());
}

// ---------------------------------------------------------------------------
// Input assembly
// ---------------------------------------------------------------------------

void D3D12CommandList::SetVertexBuffer(Buffer* vb)
{
	DYNAMIC_ASSERT(vb, "D3D12CommandList::SetVertexBuffer: vb is null");
	D3D12_VERTEX_BUFFER_VIEW vbv = static_cast<D3D12Buffer*>(vb)->GetVertexBufferView();
	m_list->IASetVertexBuffers(0, 1, &vbv);
}

void D3D12CommandList::SetIndexBuffer(Buffer* ib)
{
	DYNAMIC_ASSERT(ib, "D3D12CommandList::SetIndexBuffer: ib is null");
	D3D12_INDEX_BUFFER_VIEW ibv = static_cast<D3D12Buffer*>(ib)->GetIndexBufferView();
	m_list->IASetIndexBuffer(&ibv);
}

void D3D12CommandList::SetPrimitiveTopology(PrimitiveTopology topo)
{
	m_list->IASetPrimitiveTopology(ToD3D12Topology(topo));
}

// ---------------------------------------------------------------------------
// Viewport & scissor
// ---------------------------------------------------------------------------

void D3D12CommandList::SetViewport(f32 x, f32 y, f32 width, f32 height,
                                    f32 minDepth, f32 maxDepth)
{
	D3D12_VIEWPORT vp;
	vp.TopLeftX = x;
	vp.TopLeftY = y;
	vp.Width    = width;
	vp.Height   = height;
	vp.MinDepth = minDepth;
	vp.MaxDepth = maxDepth;
	m_list->RSSetViewports(1, &vp);
}

void D3D12CommandList::SetScissorRect(u32 left, u32 top, u32 right, u32 bottom)
{
	D3D12_RECT rect;
	rect.left   = static_cast<LONG>(left);
	rect.top    = static_cast<LONG>(top);
	rect.right  = static_cast<LONG>(right);
	rect.bottom = static_cast<LONG>(bottom);
	m_list->RSSetScissorRects(1, &rect);
}

// ---------------------------------------------------------------------------
// Render output binding
// ---------------------------------------------------------------------------

void D3D12CommandList::SetRenderTargets(u32 rtvCount, const DescriptorHandle* rtvs,
                                         DescriptorHandle dsv)
{
	DYNAMIC_ASSERT(rtvCount <= 8, "D3D12CommandList::SetRenderTargets: max 8 RTVs");

	D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvs[8] = {};
	for (u32 i = 0; i < rtvCount; ++i)
	{
		d3dRtvs[i].ptr = static_cast<SIZE_T>(rtvs[i].ptr);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE d3dDsv = {};
	d3dDsv.ptr = static_cast<SIZE_T>(dsv.ptr);

	m_list->OMSetRenderTargets(rtvCount, d3dRtvs, FALSE,
	                            dsv.IsValid() ? &d3dDsv : nullptr);
}

void D3D12CommandList::ClearRenderTarget(DescriptorHandle rtv,
                                          f32 r, f32 g, f32 b, f32 a)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handle = {};
	handle.ptr = static_cast<SIZE_T>(rtv.ptr);
	const float color[] = { r, g, b, a };
	m_list->ClearRenderTargetView(handle, color, 0, nullptr);
}

void D3D12CommandList::ClearDepthStencil(DescriptorHandle dsv, f32 depth, u8 stencil)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handle = {};
	handle.ptr = static_cast<SIZE_T>(dsv.ptr);
	m_list->ClearDepthStencilView(handle,
	    D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
	    depth, stencil, 0, nullptr);
}

// ---------------------------------------------------------------------------
// Resource binding
// ---------------------------------------------------------------------------

void D3D12CommandList::SetConstantBuffer(u32 rootIndex, Buffer* buffer)
{
	DYNAMIC_ASSERT(buffer, "D3D12CommandList::SetConstantBuffer: buffer is null");
	m_list->SetGraphicsRootConstantBufferView(
		rootIndex, static_cast<D3D12Buffer*>(buffer)->GetGPUAddress());
}

void D3D12CommandList::SetConstantBufferView(u32 rootIndex, Buffer* buffer, u64 offset, u64 /*size*/)
{
	DYNAMIC_ASSERT(buffer, "D3D12CommandList::SetConstantBufferView: buffer is null");
	D3D12_GPU_VIRTUAL_ADDRESS addr = static_cast<D3D12Buffer*>(buffer)->GetGPUAddress() + offset;
	m_list->SetGraphicsRootConstantBufferView(rootIndex, addr);
}

void D3D12CommandList::SetShaderResource(u32 rootIndex, Texture* texture)
{
	DYNAMIC_ASSERT(texture,   "D3D12CommandList::SetShaderResource: texture is null");
	DYNAMIC_ASSERT(m_srvHeap, "D3D12CommandList::SetShaderResource: no SRV heap — call SetSRVHeap() before recording");
	DYNAMIC_ASSERT(m_device,  "D3D12CommandList::SetShaderResource: no device — call SetDevice() before recording");

	DescriptorHandle srcHandle = texture->GetSRV();
	FATAL_ASSERT(srcHandle.IsValid(),
	    "D3D12CommandList::SetShaderResource: texture has no SRV — use TextureUsage::Sampled or RenderTarget");

	D3D12_CPU_DESCRIPTOR_HANDLE src = { static_cast<SIZE_T>(srcHandle.ptr) };
	D3D12DescriptorHeap::Allocation alloc = m_srvHeap->Alloc(1);
	m_device->CopyDescriptorsSimple(1, alloc.cpu, src, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	m_list->SetGraphicsRootDescriptorTable(rootIndex, alloc.gpu);
}

void D3D12CommandList::SetShaderResources(u32 rootIndex, const Vector<Texture*>& textures)
{
	DYNAMIC_ASSERT(!textures.empty(), "D3D12CommandList::SetShaderResources: textures is empty");
	DYNAMIC_ASSERT(m_srvHeap, "D3D12CommandList::SetShaderResources: no SRV heap — call SetSRVHeap() before recording");
	DYNAMIC_ASSERT(m_device,  "D3D12CommandList::SetShaderResources: no device — call SetDevice() before recording");

	const u32 count = static_cast<u32>(textures.size());
	D3D12DescriptorHeap::Allocation alloc = m_srvHeap->Alloc(count);
	const u32 stride = m_srvHeap->GetDescriptorSize();

	for (u32 i = 0; i < count; ++i)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE dst = { alloc.cpu.ptr + static_cast<SIZE_T>(i) * stride };

		if (textures[i] == nullptr)
		{
			// Write a null SRV — D3D12 spec guarantees sampling a null SRV returns 0.
			// Used when a texture slot is valid in the material but not yet uploaded.
			D3D12_SHADER_RESOURCE_VIEW_DESC nullDesc          = {};
			nullDesc.Format                                   = DXGI_FORMAT_R8G8B8A8_UNORM;
			nullDesc.ViewDimension                            = D3D12_SRV_DIMENSION_TEXTURE2D;
			nullDesc.Shader4ComponentMapping                  = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			nullDesc.Texture2D.MipLevels                      = 1;
			m_device->CreateShaderResourceView(nullptr, &nullDesc, dst);
			continue;
		}

		DescriptorHandle srcHandle = textures[i]->GetSRV();
		FATAL_ASSERT(srcHandle.IsValid(),
		    "D3D12CommandList::SetShaderResources: texture has no SRV — use TextureUsage::Sampled or RenderTarget");

		D3D12_CPU_DESCRIPTOR_HANDLE src = { static_cast<SIZE_T>(srcHandle.ptr) };
		m_device->CopyDescriptorsSimple(1, dst, src, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	m_list->SetGraphicsRootDescriptorTable(rootIndex, alloc.gpu);
}

void D3D12CommandList::SetShaderResourceBuffer(u32 rootIndex, Buffer* buffer, u64 offset)
{
	DYNAMIC_ASSERT(buffer, "D3D12CommandList::SetShaderResourceBuffer: buffer is null");
	D3D12_GPU_VIRTUAL_ADDRESS address = static_cast<D3D12Buffer*>(buffer)->GetGPUAddress() + offset;
	m_list->SetGraphicsRootShaderResourceView(rootIndex, address);
}

// ---------------------------------------------------------------------------
// Copy / transfer
// ---------------------------------------------------------------------------

void D3D12CommandList::CopyBuffer(Buffer* src, Buffer* dst,
                                   u64 srcOffset, u64 dstOffset, u64 size)
{
	DYNAMIC_ASSERT(src, "D3D12CommandList::CopyBuffer: src is null");
	DYNAMIC_ASSERT(dst, "D3D12CommandList::CopyBuffer: dst is null");

	D3D12Buffer* d3dSrc = static_cast<D3D12Buffer*>(src);
	D3D12Buffer* d3dDst = static_cast<D3D12Buffer*>(dst);

	m_list->CopyBufferRegion(d3dDst->GetNativeResource(), dstOffset,
	                         d3dSrc->GetNativeResource(), srcOffset, size);
}

void D3D12CommandList::CopyBufferToTexture(Buffer* src, u64 srcOffset, u32 srcRowPitch,
                                            Texture* dst, u32 mipLevel, u32 arraySlice)
{
	DYNAMIC_ASSERT(src, "D3D12CommandList::CopyBufferToTexture: src is null");
	DYNAMIC_ASSERT(dst, "D3D12CommandList::CopyBufferToTexture: dst is null");
	DYNAMIC_ASSERT(srcRowPitch > 0, "D3D12CommandList::CopyBufferToTexture: srcRowPitch must be > 0");

	D3D12Buffer*  d3dSrc = static_cast<D3D12Buffer*>(src);
	D3D12Texture* d3dDst = static_cast<D3D12Texture*>(dst);

	const u32 fullWidth  = d3dDst->GetWidth();
	const u32 fullHeight = d3dDst->GetHeight();
	const u32 mipWidth   = (fullWidth  >> mipLevel) > 0 ? (fullWidth  >> mipLevel) : 1;
	const u32 mipHeight  = (fullHeight >> mipLevel) > 0 ? (fullHeight >> mipLevel) : 1;

	const TextureFormat format = d3dDst->GetFormat();

	// srcRowPitch is pre-aligned by ResourceManager — use directly.
	const u32 rowPitch = srcRowPitch;

	// Source location: placed footprint in the staging buffer.
	D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
	srcLoc.pResource                          = d3dSrc->GetNativeResource();
	srcLoc.Type                               = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	srcLoc.PlacedFootprint.Offset             = srcOffset;
	srcLoc.PlacedFootprint.Footprint.Format   = ToD3D12Format(format);
	srcLoc.PlacedFootprint.Footprint.Width    = mipWidth;
	srcLoc.PlacedFootprint.Footprint.Height   = mipHeight;
	srcLoc.PlacedFootprint.Footprint.Depth    = 1;
	srcLoc.PlacedFootprint.Footprint.RowPitch = rowPitch;

	// Destination location: subresource index.
	D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
	dstLoc.pResource        = d3dDst->GetNativeResource();
	dstLoc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dstLoc.SubresourceIndex = mipLevel + (arraySlice * d3dDst->GetMipLevels());

	m_list->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);
}

// ---------------------------------------------------------------------------
// Resource transitions
// ---------------------------------------------------------------------------

void D3D12CommandList::TransitionResource(ID3D12Resource* resource,
                                          D3D12_RESOURCE_STATES before,
                                          D3D12_RESOURCE_STATES after)
{
	if (before == after)
		return;

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource   = resource;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = before;
	barrier.Transition.StateAfter  = after;
	m_list->ResourceBarrier(1, &barrier);
}

void D3D12CommandList::TransitionTexture(Texture* texture, ResourceState newState)
{
	DYNAMIC_ASSERT(texture, "D3D12CommandList::TransitionTexture: texture is null");
	D3D12Texture* d3dTexture = static_cast<D3D12Texture*>(texture);

	D3D12_RESOURCE_STATES after = ToD3D12ResourceState(newState);
	TransitionResource(d3dTexture->GetNativeResource(), d3dTexture->GetCurrentState(), after);
	d3dTexture->SetCurrentState(after);
}

void D3D12CommandList::TransitionBuffer(Buffer* buffer, ResourceState newState)
{
	DYNAMIC_ASSERT(buffer, "D3D12CommandList::TransitionBuffer: buffer is null");
	D3D12Buffer* d3dBuffer = static_cast<D3D12Buffer*>(buffer);

	// Upload-heap buffers must stay in GENERIC_READ — skip the barrier.
	if (d3dBuffer->IsUploadHeap())
		return;

	D3D12_RESOURCE_STATES after = ToD3D12ResourceState(newState);
	TransitionResource(d3dBuffer->GetNativeResource(), d3dBuffer->GetCurrentState(), after);
	d3dBuffer->SetCurrentState(after);
}

// ---------------------------------------------------------------------------
// Draw / dispatch
// ---------------------------------------------------------------------------

void D3D12CommandList::Draw(u32 vertexCount, u32 instanceCount,
                             u32 firstVertex, u32 firstInstance)
{
	m_list->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
}

void D3D12CommandList::DrawIndexed(u32 indexCount, u32 instanceCount,
                                    u32 firstIndex, u32 baseVertex, u32 firstInstance)
{
	m_list->DrawIndexedInstanced(indexCount, instanceCount, firstIndex,
	                              static_cast<INT>(baseVertex), firstInstance);
}

void D3D12CommandList::Dispatch(u32 x, u32 y, u32 z)
{
	m_list->Dispatch(x, y, z);
}

#endif
