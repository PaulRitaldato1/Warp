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
		case TextureUsage::DepthStencilSampled:
			resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
			// Use typeless resource format so both DSV (D32_FLOAT) and SRV (R32_FLOAT) views work.
			if (desc.format == TextureFormat::Depth32F)
			{
				resourceDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			}
			else if (desc.format == TextureFormat::Depth24Stencil8)
			{
				resourceDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
			}
			initState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
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

	// ---- Descriptor creation ----

	// RTV — RenderTarget textures
	if (desc.usage == TextureUsage::RenderTarget)
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		heapDesc.NumDescriptors = 1;
		heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_rtvHeap)));

		D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
		device->CreateRenderTargetView(m_resource.Get(), nullptr, handle);
		m_rtvHandle = { static_cast<u64>(handle.ptr), desc.width, desc.height };
	}

	// DSV — DepthStencil and DepthStencilSampled textures
	if (desc.usage == TextureUsage::DepthStencil || desc.usage == TextureUsage::DepthStencilSampled)
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		heapDesc.NumDescriptors = 1;
		heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_dsvHeap)));

		D3D12_CPU_DESCRIPTOR_HANDLE handle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

		if (desc.usage == TextureUsage::DepthStencilSampled)
		{
			// Resource is typeless — must specify the typed depth format for the DSV.
			D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			dsvDesc.Format        = ToD3D12Format(desc.format);
			device->CreateDepthStencilView(m_resource.Get(), &dsvDesc, handle);
		}
		else
		{
			device->CreateDepthStencilView(m_resource.Get(), nullptr, handle);
		}
		m_dsvHandle = { static_cast<u64>(handle.ptr), desc.width, desc.height };
	}

	// SRV — Sampled, RenderTarget, and DepthStencilSampled textures.
	// CPU-only staging heap; copy into the frame's shader-visible heap when binding to a root signature.
	if (desc.usage == TextureUsage::Sampled || desc.usage == TextureUsage::RenderTarget
		|| desc.usage == TextureUsage::DepthStencilSampled)
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.NumDescriptors = 1;
		heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // CPU-only staging
		ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_srvHeap)));

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping   = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		// Typeless depth resources need an explicit float format for the SRV.
		if (desc.usage == TextureUsage::DepthStencilSampled)
		{
			if (desc.format == TextureFormat::Depth32F)
			{
				srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
			}
			else if (desc.format == TextureFormat::Depth24Stencil8)
			{
				srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			}
		}
		else
		{
			srvDesc.Format = resourceDesc.Format;
		}

		if (desc.type == TextureType::CubeMap)
		{
			srvDesc.ViewDimension               = D3D12_SRV_DIMENSION_TEXTURECUBE;
			srvDesc.TextureCube.MipLevels       = desc.mipLevels;
			srvDesc.TextureCube.MostDetailedMip = 0;
		}
		else
		{
			srvDesc.ViewDimension             = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels       = desc.mipLevels;
			srvDesc.Texture2D.MostDetailedMip = 0;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE handle = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
		device->CreateShaderResourceView(m_resource.Get(), &srvDesc, handle);
		m_srvHandle = { static_cast<u64>(handle.ptr), desc.width, desc.height };
	}

	LOG_DEBUG("D3D12Texture created: {}x{}, mips={}", desc.width, desc.height, desc.mipLevels);
}

void D3D12Texture::Init(const TextureDesc& /*desc*/)
{
	// Use InitializeWithDevice — needs a device pointer.
}

void D3D12Texture::Cleanup()
{
	m_rtvHeap.Reset();
	m_dsvHeap.Reset();
	m_srvHeap.Reset();
	m_resource.Reset();
}

#endif
