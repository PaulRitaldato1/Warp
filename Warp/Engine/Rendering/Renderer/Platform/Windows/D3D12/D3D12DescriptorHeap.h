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

	void Initialize(ID3D12Device* device, u32 capacity)
	{
		DYNAMIC_ASSERT(device,   "D3D12DescriptorHeap::Initialize: device is null");
		DYNAMIC_ASSERT(capacity, "D3D12DescriptorHeap::Initialize: capacity must be > 0");

		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NumDescriptors = capacity;
		desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_heap)));

		m_capacity       = capacity;
		m_offset         = 0;
		m_descriptorSize = device->GetDescriptorHandleIncrementSize(
		                       D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		m_cpuStart       = m_heap->GetCPUDescriptorHandleForHeapStart();
		m_gpuStart       = m_heap->GetGPUDescriptorHandleForHeapStart();
	}

	// Allocate count consecutive descriptors and return the first handle.
	Allocation Alloc(u32 count = 1)
	{
		DYNAMIC_ASSERT(m_offset + count <= m_capacity,
		               "D3D12DescriptorHeap: out of descriptor space");
		Allocation result;
		result.cpu.ptr = m_cpuStart.ptr + static_cast<SIZE_T>(m_offset) * m_descriptorSize;
		result.gpu.ptr = m_gpuStart.ptr + static_cast<u64>(m_offset)    * m_descriptorSize;
		m_offset += count;
		return result;
	}

	// Reclaim all slots — call at the start of every frame.
	void Reset() { m_offset = 0; }

	bool IsInitialized() const { return m_heap != nullptr; }

	ID3D12DescriptorHeap* GetNative()         const { return m_heap.Get(); }
	u32                   GetDescriptorSize() const { return m_descriptorSize; }

private:
	ComRef<ID3D12DescriptorHeap> m_heap;
	u32                          m_capacity       = 0;
	u32                          m_offset         = 0;
	u32                          m_descriptorSize = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE  m_cpuStart       = {};
	D3D12_GPU_DESCRIPTOR_HANDLE  m_gpuStart       = {};
};

#endif
