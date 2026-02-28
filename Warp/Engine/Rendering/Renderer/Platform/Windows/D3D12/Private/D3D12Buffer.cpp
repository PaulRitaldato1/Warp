#ifdef WARP_BUILD_DX12

#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12Buffer.h>
#include <Debugging/Assert.h>
#include <Debugging/Logging.h>

#include <cstring>

void D3D12Buffer::InitializeWithDevice(ID3D12Device* device, const BufferDesc& desc)
{
	DYNAMIC_ASSERT(device,               "D3D12Buffer::InitializeWithDevice: device is null");
	DYNAMIC_ASSERT(desc.numElements > 0, "D3D12Buffer::InitializeWithDevice: numElements must be > 0");
	DYNAMIC_ASSERT(desc.stride > 0,      "D3D12Buffer::InitializeWithDevice: stride must be > 0");

	m_size   = static_cast<u64>(desc.numElements) * desc.stride;
	m_stride = desc.stride;
	m_usage  = desc.usage;

	// All buffers currently use the upload heap so the CPU can write to them
	// directly via Map/Unmap. Static buffers should ideally use a default heap
	// + a staging copy via the copy queue — this can be revisited once a
	// resource upload pipeline is in place.
	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type                  = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width              = m_size;
	resourceDesc.Height             = 1;
	resourceDesc.DepthOrArraySize   = 1;
	resourceDesc.MipLevels          = 1;
	resourceDesc.Format             = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count   = 1;
	resourceDesc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ThrowIfFailed(device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_resource)));

	m_gpuAddr      = m_resource->GetGPUVirtualAddress();
	m_isUploadHeap = (heapProps.Type == D3D12_HEAP_TYPE_UPLOAD);
	m_currentState = m_isUploadHeap ? D3D12_RESOURCE_STATE_GENERIC_READ
	                                : D3D12_RESOURCE_STATE_COMMON;

	if (!desc.name.empty())
	{
		WString wname(desc.name.begin(), desc.name.end());
		m_resource->SetName(wname.c_str());
	}

	if (desc.initialData)
	{
		UploadData(desc.initialData, m_size);
	}

	LOG_DEBUG("D3D12Buffer created: " + desc.name +
			  " (" + std::to_string(m_size) + " bytes)");
}

void D3D12Buffer::UploadData(const void* data, size_t size)
{
	DYNAMIC_ASSERT(m_resource, "D3D12Buffer::UploadData: buffer not initialized");
	DYNAMIC_ASSERT(data,       "D3D12Buffer::UploadData: data is null");
	DYNAMIC_ASSERT(size <= m_size, "D3D12Buffer::UploadData: size exceeds buffer capacity");

	void* mapped = Map();
	std::memcpy(mapped, data, size);
	Unmap();
}

void* D3D12Buffer::Map()
{
	DYNAMIC_ASSERT(m_resource, "D3D12Buffer::Map: buffer not initialized");

	D3D12_RANGE readRange = { 0, 0 }; // CPU does not read from this buffer
	void* ptr = nullptr;
	ThrowIfFailed(m_resource->Map(0, &readRange, &ptr));
	return ptr;
}

void D3D12Buffer::Unmap()
{
	DYNAMIC_ASSERT(m_resource, "D3D12Buffer::Unmap: buffer not initialized");
	m_resource->Unmap(0, nullptr);
}

D3D12_VERTEX_BUFFER_VIEW D3D12Buffer::GetVertexBufferView() const
{
	D3D12_VERTEX_BUFFER_VIEW vbv;
	vbv.BufferLocation = m_gpuAddr;
	vbv.SizeInBytes    = static_cast<UINT>(m_size);
	vbv.StrideInBytes  = m_stride;
	return vbv;
}

D3D12_INDEX_BUFFER_VIEW D3D12Buffer::GetIndexBufferView() const
{
	// Engine indices are always u32.
	D3D12_INDEX_BUFFER_VIEW ibv;
	ibv.BufferLocation = m_gpuAddr;
	ibv.SizeInBytes    = static_cast<UINT>(m_size);
	ibv.Format         = DXGI_FORMAT_R32_UINT;
	return ibv;
}

#endif
