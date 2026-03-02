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
	m_device = device;

	// All buffers live on the default (GPU-only) heap.
	// Data must be uploaded via UploadData() which creates a staging buffer
	// and returns a PendingStagingUpload for the Renderer to record the copy.
	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type                  = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width              = m_size;
	resourceDesc.Height             = 1;
	resourceDesc.DepthOrArraySize   = 1;
	resourceDesc.MipLevels          = 1;
	resourceDesc.Format             = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count   = 1;
	resourceDesc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	m_currentState = D3D12_RESOURCE_STATE_COMMON;

	ThrowIfFailed(device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		m_currentState,
		nullptr,
		IID_PPV_ARGS(&m_resource)));

	m_gpuAddr          = m_resource->GetGPUVirtualAddress();
	m_isStagingBuffer  = false;

	if (!desc.name.empty())
	{
		WString wname(desc.name.begin(), desc.name.end());
		m_resource->SetName(wname.c_str());
	}

	LOG_DEBUG("D3D12Buffer created: {} ({} bytes, GPU-only)", desc.name, m_size);
}

URef<D3D12Buffer> D3D12Buffer::CreateStagingBuffer(ID3D12Device* device, u64 size)
{
	URef<D3D12Buffer> staging = std::make_unique<D3D12Buffer>();
	staging->m_size           = size;
	staging->m_stride         = 0;
	staging->m_device         = device;
	staging->m_isStagingBuffer = true;
	staging->m_currentState   = D3D12_RESOURCE_STATE_GENERIC_READ;

	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type                  = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width              = size;
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
		IID_PPV_ARGS(&staging->m_resource)));

	staging->m_gpuAddr = staging->m_resource->GetGPUVirtualAddress();

	return staging;
}

PendingStagingUpload D3D12Buffer::UploadData(const void* data, size_t size)
{
	DYNAMIC_ASSERT(m_resource, "D3D12Buffer::UploadData: buffer not initialized");
	DYNAMIC_ASSERT(data,       "D3D12Buffer::UploadData: data is null");
	DYNAMIC_ASSERT(size <= m_size, "D3D12Buffer::UploadData: size exceeds buffer capacity");
	DYNAMIC_ASSERT(!m_isStagingBuffer, "D3D12Buffer::UploadData: cannot upload to a staging buffer");

	URef<D3D12Buffer> staging = CreateStagingBuffer(m_device, size);

	void* mapped = staging->Map();
	std::memcpy(mapped, data, size);
	staging->Unmap();

	PendingStagingUpload upload;
	upload.stagingBuffer = std::move(staging);
	upload.destination   = this;
	upload.size          = static_cast<u64>(size);
	return upload;
}

void* D3D12Buffer::Map()
{
	DYNAMIC_ASSERT(m_resource, "D3D12Buffer::Map: buffer not initialized");
	DYNAMIC_ASSERT(m_isStagingBuffer, "D3D12Buffer::Map: only staging buffers can be mapped");

	D3D12_RANGE readRange = { 0, 0 }; // CPU does not read from this buffer
	void* ptr = nullptr;
	ThrowIfFailed(m_resource->Map(0, &readRange, &ptr));
	return ptr;
}

void D3D12Buffer::Unmap()
{
	DYNAMIC_ASSERT(m_resource, "D3D12Buffer::Unmap: buffer not initialized");
	DYNAMIC_ASSERT(m_isStagingBuffer, "D3D12Buffer::Unmap: only staging buffers can be unmapped");
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
