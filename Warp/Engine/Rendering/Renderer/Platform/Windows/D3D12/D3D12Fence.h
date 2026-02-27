#pragma once

#include <Rendering/Renderer/Fence.h>

#ifdef WARP_WINDOWS

#include <d3d12.h>

class D3D12Fence : public Fence
{
public:
	~D3D12Fence();

	void InitializeWithDevice(ID3D12Device* device);

	// Fence interface
	void Initialize() override;
	void Signal() override;
	void Wait(u64 timeout = UINT64_MAX) override;
	u64  GetCompletedValue() const override;
	void Reset() override;

	// D3D12-specific helpers
	// Signal the fence from the GPU side via a command queue
	void GPUSignal(ID3D12CommandQueue* queue);
	// Block the CPU until the fence reaches the specified value
	void WaitForValue(u64 value, u64 timeout = UINT64_MAX);

	u64			 GetNextValue() const { return m_nextValue; }
	ID3D12Fence* GetNative() const { return m_fence.Get(); }

private:
	ComRef<ID3D12Fence> m_fence;
	HANDLE				m_event		 = nullptr;
	u64					m_nextValue	 = 1;
};

#endif
