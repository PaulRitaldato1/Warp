#pragma once

#include <Rendering/Renderer/Buffer.h>

#ifdef WARP_WINDOWS

#include <d3d12.h>

class D3D12Buffer : public Buffer
{
public:
	// Device-specific init — called by D3D12Device::CreateBuffer().
	// Creates the buffer on D3D12_HEAP_TYPE_DEFAULT (GPU-only).
	void InitializeWithDevice(ID3D12Device* device, const BufferDesc& desc);

	// Buffer interface
	PendingStagingUpload UploadData(const void* data, size_t size) override;
	void* Map()   override;
	void  Unmap() override;
	u64   GetSize()          const override { return m_size; }
	void* GetNativeHandle()  const override { return m_resource.Get(); }

	// D3D12-specific view helpers — used by D3D12CommandList
	D3D12_VERTEX_BUFFER_VIEW  GetVertexBufferView() const;
	D3D12_INDEX_BUFFER_VIEW   GetIndexBufferView()  const;
	D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress()       const { return m_gpuAddr; }

	// State tracking for barrier insertion.
	// Staging buffers are permanently GENERIC_READ; transitions are skipped.
	ID3D12Resource*       GetNativeResource() const { return m_resource.Get(); }
	D3D12_RESOURCE_STATES GetCurrentState()   const { return m_currentState; }
	void                  SetCurrentState(D3D12_RESOURCE_STATES s) { m_currentState = s; }
	bool                  IsUploadHeap()      const { return m_isStagingBuffer; }

private:
	// Creates a staging-only buffer on D3D12_HEAP_TYPE_UPLOAD.
	// Used internally by UploadData() to create the temporary staging resource.
	static URef<D3D12Buffer> CreateStagingBuffer(ID3D12Device* device, u64 size);

	ComRef<ID3D12Resource>    m_resource;
	D3D12_GPU_VIRTUAL_ADDRESS m_gpuAddr       = 0;
	u64                       m_size           = 0;
	u32                       m_stride         = 0;
	D3D12_RESOURCE_STATES     m_currentState   = D3D12_RESOURCE_STATE_COMMON;
	ID3D12Device*             m_device         = nullptr;
	bool                      m_isStagingBuffer = false;
};

#endif
