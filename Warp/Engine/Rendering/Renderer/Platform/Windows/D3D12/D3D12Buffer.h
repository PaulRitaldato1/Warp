#pragma once

#include <Rendering/Renderer/Buffer.h>

#ifdef WARP_WINDOWS

#include <d3d12.h>

class D3D12Buffer : public Buffer
{
public:
	// Device-specific init — called by D3D12Device::CreateBuffer().
	void InitializeWithDevice(ID3D12Device* device, const BufferDesc& desc);

	// Buffer interface
	void  UploadData(const void* data, size_t size) override;
	void* Map()   override;
	void  Unmap() override;
	u64   GetSize()          const override { return m_size; }
	void* GetNativeHandle()  const override { return m_resource.Get(); }

	// D3D12-specific view helpers — used by D3D12CommandList
	D3D12_VERTEX_BUFFER_VIEW  GetVertexBufferView() const;
	D3D12_INDEX_BUFFER_VIEW   GetIndexBufferView()  const;
	D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress()       const { return m_gpuAddr; }

	// State tracking for barrier insertion.
	// Upload-heap buffers are permanently GENERIC_READ; transitions are skipped.
	ID3D12Resource*       GetNativeResource() const { return m_resource.Get(); }
	D3D12_RESOURCE_STATES GetCurrentState()   const { return m_currentState; }
	void                  SetCurrentState(D3D12_RESOURCE_STATES s) { m_currentState = s; }
	bool                  IsUploadHeap()      const { return m_isUploadHeap; }

private:
	ComRef<ID3D12Resource>    m_resource;
	D3D12_GPU_VIRTUAL_ADDRESS m_gpuAddr      = 0;
	u64                       m_size         = 0;
	u32                       m_stride       = 0;
	D3D12_RESOURCE_STATES     m_currentState = D3D12_RESOURCE_STATE_GENERIC_READ;
	bool                      m_isUploadHeap = true;
};

#endif
