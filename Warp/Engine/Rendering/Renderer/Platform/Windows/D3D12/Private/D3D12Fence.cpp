#ifdef WARP_BUILD_DX12

#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12Fence.h>
#include <Debugging/Logging.h>
#include <Debugging/Assert.h>

D3D12Fence::~D3D12Fence()
{
	if (m_event)
	{
		CloseHandle(m_event);
		m_event = nullptr;
	}
}

void D3D12Fence::InitializeWithDevice(ID3D12Device* device)
{
	DYNAMIC_ASSERT(device, "D3D12Fence::InitializeWithDevice: device is null");

	ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));

	m_event = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
	DYNAMIC_ASSERT(m_event, "D3D12Fence: CreateEventEx failed");

	m_nextValue = 1;
}

void D3D12Fence::Initialize()
{
	// CPU-side signal back to 0 — only valid before GPU work starts
	if (m_fence)
	{
		ThrowIfFailed(m_fence->Signal(0));
		m_nextValue = 1;
	}
}

void D3D12Fence::Signal()
{
	// CPU-side signal
	DYNAMIC_ASSERT(m_fence, "D3D12Fence::Signal: fence not initialized");
	ThrowIfFailed(m_fence->Signal(m_nextValue++));
}

void D3D12Fence::Wait(u64 timeout)
{
	WaitForValue(m_nextValue - 1, timeout);
}

u64 D3D12Fence::GetCompletedValue() const
{
	DYNAMIC_ASSERT(m_fence, "D3D12Fence::GetCompletedValue: fence not initialized");
	return m_fence->GetCompletedValue();
}

void D3D12Fence::Reset()
{
	m_nextValue = 1;
}

void D3D12Fence::GPUSignal(ID3D12CommandQueue* queue)
{
	DYNAMIC_ASSERT(queue, "D3D12Fence::GPUSignal: queue is null");
	DYNAMIC_ASSERT(m_fence, "D3D12Fence::GPUSignal: fence not initialized");
	ThrowIfFailed(queue->Signal(m_fence.Get(), m_nextValue++));
}

void D3D12Fence::WaitForValue(u64 value, u64 timeout)
{
	DYNAMIC_ASSERT(m_fence, "D3D12Fence::WaitForValue: fence not initialized");

	if (m_fence->GetCompletedValue() < value)
	{
		ThrowIfFailed(m_fence->SetEventOnCompletion(value, m_event));
		WaitForSingleObjectEx(m_event, static_cast<DWORD>(timeout == UINT64_MAX ? INFINITE : timeout), FALSE);
	}
}

#endif
