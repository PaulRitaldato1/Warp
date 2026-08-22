#pragma once

#ifdef WARP_WINDOWS

#include <Common/CommonTypes.h>
#include <Debugging/Assert.h>
#include <d3d12.h>

// ---------------------------------------------------------------------------
// Shader-visible CBV/SRV/UAV descriptor heap with a simple linear allocator.
//
// Usage:
//   1. Call Initialize() once with the desired capacity.
//   2. Call Reset() at the start of each frame to reclaim all slots.
//   3. Call Alloc() to carve out consecutive descriptors.
//   4. Bind via ID3D12GraphicsCommandList::SetDescriptorHeaps() (done in
//      D3D12CommandList::Begin() when a heap is attached).
// ---------------------------------------------------------------------------
class D3D12DescriptorHeap
{
public:
	struct Allocation
	{
		D3D12_CPU_DESCRIPTOR_HANDLE cpu = {};
		D3D12_GPU_DESCRIPTOR_HANDLE gpu = {};
	};

	void Initialize(ID3D12Device* device, u32 capacity, u32 framesInFlight = 1)
	{
		DYNAMIC_ASSERT(device,   "D3D12DescriptorHeap::Initialize: device is null");
		DYNAMIC_ASSERT(capacity, "D3D12DescriptorHeap::Initialize: capacity must be > 0");
		DYNAMIC_ASSERT(framesInFlight, "D3D12DescriptorHeap::Initialize: framesInFlight must be > 0");

		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NumDescriptors = capacity;
		desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_heap)));

		m_capacity       = capacity;
		m_framesInFlight = framesInFlight;
		m_regionSize     = capacity / framesInFlight;
		m_base           = 0;
		m_offset         = 0;
		m_descriptorSize = device->GetDescriptorHandleIncrementSize(
		                       D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		m_cpuStart       = m_heap->GetCPUDescriptorHandleForHeapStart();
		m_gpuStart       = m_heap->GetGPUDescriptorHandleForHeapStart();
	}

	// Allocate count consecutive descriptors and return the first handle.
	Allocation Alloc(u32 count = 1)
	{
		DYNAMIC_ASSERT(m_offset + count <= m_regionSize,
		               "D3D12DescriptorHeap: out of descriptor space");
		const u32 slot = m_base + m_offset;

		Allocation result;
		result.cpu.ptr = m_cpuStart.ptr + static_cast<SIZE_T>(slot) * m_descriptorSize;
		result.gpu.ptr = m_gpuStart.ptr + static_cast<u64>(slot)    * m_descriptorSize;
		m_offset += count;
		return result;
	}

	// Point the allocator at this frame's region. The GPU may still be reading the
	// previous frames' descriptors, so recording must not reuse their slots.
	// Idempotent within a frame: several command lists Begin() against one heap.
	void BeginFrame(u32 frameIndex)
	{
		if (frameIndex == m_currentFrame)
		{
			return;
		}

		m_currentFrame = frameIndex;
		m_base         = (frameIndex % m_framesInFlight) * m_regionSize;
		m_offset       = 0;
	}

	bool IsInitialized() const { return m_heap != nullptr; }

	ID3D12DescriptorHeap* GetNative()         const { return m_heap.Get(); }
	u32                   GetDescriptorSize() const { return m_descriptorSize; }

private:
	ComRef<ID3D12DescriptorHeap> m_heap;
	u32                          m_capacity       = 0;
	u32                          m_framesInFlight = 1;
	u32                          m_regionSize     = 0;
	u32                          m_base           = 0;
	u32                          m_currentFrame   = ~0u;
	u32                          m_offset         = 0;
	u32                          m_descriptorSize = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE  m_cpuStart       = {};
	D3D12_GPU_DESCRIPTOR_HANDLE  m_gpuStart       = {};
};

#endif
