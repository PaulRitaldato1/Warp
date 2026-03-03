#pragma once

#include <Rendering/Renderer/UploadBuffer.h>
#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12Buffer.h>

#ifdef WARP_WINDOWS

#include <d3d12.h>

// ---------------------------------------------------------------------------
// D3D12UploadBuffer — host-visible ring buffer for per-frame constant data
// and as a source for CopyBuffer re-uploads.  Backed by a D3D12Buffer
// created on D3D12_HEAP_TYPE_UPLOAD (staging).
// ---------------------------------------------------------------------------

class D3D12UploadBuffer : public UploadBuffer
{
public:
	void InitializeWithDevice(ID3D12Device* device, u64 size, u32 framesInFlight);

	// UploadBuffer interface
	void             Initialize(u64 size, u32 framesInFlight) override; // no-op (use InitializeWithDevice)
	UploadAllocation Alloc(u64 size, u64 alignment = 256)     override;
	void             OnBeginFrame()                           override;
	void             Cleanup()                                override;
	Buffer*          GetBackingBuffer()                       override;

private:
	URef<D3D12Buffer> m_backingBuffer;
	void*             m_mappedPtr = nullptr;
	u64               m_gpuBase   = 0;
};

#endif
