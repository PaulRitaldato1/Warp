#ifdef WARP_BUILD_DX12

#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12CommandQueue.h>
#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12CommandList.h>
#include <Debugging/Assert.h>
#include <Debugging/Logging.h>

void D3D12CommandQueue::InitializeWithDevice(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type)
{
	DYNAMIC_ASSERT(device, "D3D12CommandQueue::InitializeWithDevice: device is null");

	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Type	  = type;
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	desc.Flags	  = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.NodeMask = 0;

	ThrowIfFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_queue)));

	m_fence = std::make_unique<D3D12Fence>();
	m_fence->InitializeWithDevice(device);

	LOG_DEBUG("D3D12CommandQueue initialized");
}

u64 D3D12CommandQueue::Submit(const Vector<CommandList*>& lists)
{
	DYNAMIC_ASSERT(m_queue, "D3D12CommandQueue::Submit: queue not initialized");
	DYNAMIC_ASSERT(!lists.empty(), "D3D12CommandQueue::Submit: empty list");

	Vector<ID3D12CommandList*> raw(lists.size());
	for (u32 i = 0; i < lists.size(); ++i)
	{
		D3D12CommandList& d3dList = static_cast<D3D12CommandList&>(*lists[i]);
		raw[i] = d3dList.GetNative();
	}

	m_queue->ExecuteCommandLists(static_cast<UINT>(raw.size()), raw.data());
	m_fence->GPUSignal(m_queue.Get());

	// GPUSignal increments m_nextValue, so the value just signaled is m_nextValue - 1.
	return m_fence->GetNextValue() - 1;
}

void D3D12CommandQueue::WaitForValue(u64 value)
{
	DYNAMIC_ASSERT(m_fence, "D3D12CommandQueue::WaitForValue: fence not initialized");
	m_fence->WaitForValue(value);
}

u64 D3D12CommandQueue::GetCompletedValue() const
{
	DYNAMIC_ASSERT(m_fence, "D3D12CommandQueue::GetCompletedValue: fence not initialized");
	return m_fence->GetCompletedValue();
}

void D3D12CommandQueue::WaitForQueue(CommandQueue& other, u64 fenceValue)
{
	DYNAMIC_ASSERT(m_queue, "D3D12CommandQueue::WaitForQueue: queue not initialized");

	D3D12CommandQueue& d3dOther = static_cast<D3D12CommandQueue&>(other);
	ThrowIfFailed(m_queue->Wait(d3dOther.m_fence->GetNative(), fenceValue));
}

void D3D12CommandQueue::Reset()
{
	WaitForIdle();
}

void D3D12CommandQueue::WaitForIdle()
{
	DYNAMIC_ASSERT(m_fence, "D3D12CommandQueue::WaitForIdle: fence not initialized");
	m_fence->WaitForValue(m_fence->GetNextValue() - 1);
}

#endif
