#ifdef WARP_BUILD_DX12

#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12Texture.h>
#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12Translate.h>
#include <Debugging/Assert.h>
#include <Debugging/Logging.h>

void D3D12Texture::InitializeWithDevice(ID3D12Device* device, const TextureDesc& desc)
{
	DYNAMIC_ASSERT(device,          "D3D12Texture::InitializeWithDevice: device is null");
	DYNAMIC_ASSERT(desc.width > 0,  "D3D12Texture::InitializeWithDevice: width must be > 0");
	DYNAMIC_ASSERT(desc.height > 0, "D3D12Texture::InitializeWithDevice: height must be > 0");

	m_desc = desc;

	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type                  = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Width            = desc.width;
	resourceDesc.Height           = desc.height;
	resourceDesc.MipLevels        = static_cast<UINT16>(desc.mipLevels);
	resourceDesc.Format           = ToD3D12Format(desc.format);
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout           = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags            = D3D12_RESOURCE_FLAG_NONE;

	switch (desc.type)
	{
		case TextureType::Texture3D:
			resourceDesc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
			resourceDesc.DepthOrArraySize = static_cast<UINT16>(desc.depth);
			break;
		case TextureType::CubeMap:
			resourceDesc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			resourceDesc.DepthOrArraySize = static_cast<UINT16>(desc.arrayLayers);
			break;
		default: // Texture2D
			resourceDesc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			resourceDesc.DepthOrArraySize = static_cast<UINT16>(desc.arrayLayers);
			break;
	}

	D3D12_RESOURCE_STATES initState = D3D12_RESOURCE_STATE_COPY_DEST;

	switch (desc.usage)
	{
		case TextureUsage::RenderTarget:
			resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
			initState           = D3D12_RESOURCE_STATE_RENDER_TARGET;
			break;
		case TextureUsage::DepthStencil:
			resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
			initState           = D3D12_RESOURCE_STATE_DEPTH_WRITE;
			break;
		case TextureUsage::Storage:
			resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			initState           = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			break;
		default: // Sampled / TransferDst — start in COPY_DEST, caller transitions after upload
			break;
	}

	ThrowIfFailed(device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		initState,
		nullptr,
		IID_PPV_ARGS(&m_resource)));

	m_currentState = initState;

	LOG_DEBUG("D3D12Texture created: " +
			  std::to_string(desc.width) + "x" + std::to_string(desc.height) +
			  ", mips=" + std::to_string(desc.mipLevels));
}

void D3D12Texture::Init(const TextureDesc& /*desc*/)
{
	// Use InitializeWithDevice — needs a device pointer.
}

void D3D12Texture::Cleanup()
{
	m_resource.Reset();
}

#endif
