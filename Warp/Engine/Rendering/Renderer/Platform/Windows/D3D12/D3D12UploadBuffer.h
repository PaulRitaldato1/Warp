#pragma once

#include <Rendering/Renderer/UploadBuffer.h>

#ifdef WARP_WINDOWS

#include <d3d12.h>

class D3D12UploadBuffer : public UploadBuffer
{
public:
	// D3D12-specific init: allocates a persistently-mapped UPLOAD heap.
	// Must be called before Initialize().
	void InitializeWithDevice(ID3D12Device* device, u64 size, u32 framesInFlight);

	// UploadBuffer interface
	void             Initialize(u64 size, u32 framesInFlight) override; // no-op (use InitializeWithDevice)
	UploadAllocation Alloc(u64 size, u64 alignment = 256) override;
	void             OnBeginFrame() override;
	void             Cleanup() override;

private:
	ComRef<ID3D12Resource> m_resource;
	void*                  m_mappedPtr = nullptr; // persistently mapped CPU address
	u64                    m_gpuBase   = 0;       // GPU virtual address of the resource start
};

#endif
