#pragma once

#include <Rendering/Renderer/CommandQueue.h>
#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12Fence.h>

#ifdef WARP_WINDOWS

#include <d3d12.h>

class D3D12CommandQueue : public CommandQueue
{
public:
	void InitializeWithDevice(ID3D12Device* device,
							  D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT);

	// CommandQueue interface
	u64  Submit(const Vector<CommandList*>& lists) override;
	void WaitForValue(u64 value) override;
	u64  GetCompletedValue() const override;
	void WaitForQueue(CommandQueue& other, u64 fenceValue) override;
	void Reset() override;

	// D3D12-specific
	void WaitForIdle();

	ID3D12CommandQueue* GetNative() const { return m_queue.Get(); }

private:
	ComRef<ID3D12CommandQueue> m_queue;
	URef<D3D12Fence>		   m_fence;
};

#endif
