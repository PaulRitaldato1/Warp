#ifdef WARP_BUILD_DX12

#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12SwapChain.h>
#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12CommandList.h>
#include <Rendering/Window/Window.h>
#include <Debugging/Assert.h>
#include <Debugging/Logging.h>

DXGI_FORMAT D3D12SwapChain::ToDXGIFormat(SwapChainFormat format)
{
	switch (format)
	{
		case SwapChainFormat::RGBA8:
			return DXGI_FORMAT_R8G8B8A8_UNORM;
		case SwapChainFormat::BGRA8:
			return DXGI_FORMAT_B8G8R8A8_UNORM;
		case SwapChainFormat::HDR10:
			return DXGI_FORMAT_R10G10B10A2_UNORM;
		default:
			return DXGI_FORMAT_R8G8B8A8_UNORM;
	}
}

void D3D12SwapChain::CreateRTVs(ID3D12Device* device)
{
	DYNAMIC_ASSERT(device, "D3D12SwapChain::CreateRTVs: device is null");

	// (Re)create the RTV descriptor heap sized for the current buffer count.
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type						= D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heapDesc.NumDescriptors				= m_bufferCount;
	heapDesc.Flags						= D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_rtvHeap)));

	m_rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
	for (u32 i = 0; i < m_bufferCount; ++i)
	{
		device->CreateRenderTargetView(m_backBuffers[i].Get(), nullptr, handle);
		handle.ptr += m_rtvDescriptorSize;
	}
}

void D3D12SwapChain::InitializeWithFactory(ID3D12Device* device, IDXGIFactory4* factory, ID3D12CommandQueue* queue,
										   const SwapChainDesc& desc)
{
	DYNAMIC_ASSERT(device, "D3D12SwapChain: device is null");
	DYNAMIC_ASSERT(factory, "D3D12SwapChain: factory is null");
	DYNAMIC_ASSERT(queue, "D3D12SwapChain: queue is null");
	DYNAMIC_ASSERT(desc.Window, "D3D12SwapChain: window is null");

	m_width		   = desc.Width;
	m_height	   = desc.Height;
	m_bufferCount  = desc.BufferCount;
	m_vsync		   = desc.bUseVsync;
	m_engineFormat = desc.Format;
	m_format	   = ToDXGIFormat(desc.Format);

	HWND hwnd = static_cast<HWND>(desc.Window->GetNativeHandle());
	DYNAMIC_ASSERT(hwnd, "D3D12SwapChain: window native handle is null");

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width					= desc.Width;
	swapChainDesc.Height				= desc.Height;
	swapChainDesc.Format				= m_format;
	swapChainDesc.Stereo				= FALSE;
	swapChainDesc.SampleDesc.Count		= 1;
	swapChainDesc.SampleDesc.Quality	= 0;
	swapChainDesc.BufferUsage			= DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount			= desc.BufferCount;
	swapChainDesc.Scaling				= DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect			= DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode				= DXGI_ALPHA_MODE_UNSPECIFIED;
	swapChainDesc.Flags					= 0;

	ComRef<IDXGISwapChain1> swapChain1;
	ThrowIfFailed(factory->CreateSwapChainForHwnd(queue, hwnd, &swapChainDesc, nullptr, nullptr, &swapChain1));

	// Disable Alt+Enter fullscreen toggle — engine handles this explicitly.
	ThrowIfFailed(factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));

	ThrowIfFailed(swapChain1.As(&m_swapChain));

	// Cache back-buffer resource pointers.
	// Flip-model back buffers start in PRESENT state after creation.
	m_backBuffers.resize(desc.BufferCount);
	m_backBufferStates.resize(desc.BufferCount, D3D12_RESOURCE_STATE_PRESENT);
	for (u32 i = 0; i < desc.BufferCount; ++i)
	{
		ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i])));
	}

	CreateRTVs(device);

	LOG_DEBUG("D3D12SwapChain initialized: {}x{}, {} buffers", desc.Width, desc.Height, desc.BufferCount);
}

void D3D12SwapChain::Initialize(const SwapChainDesc& /*desc*/)
{
	// Use InitializeWithFactory for D3D12 swap chains.
}

void D3D12SwapChain::Present()
{
	DYNAMIC_ASSERT(m_swapChain, "D3D12SwapChain::Present: swap chain not initialized");
	UINT syncInterval = m_vsync ? 1 : 0;
	HRESULT hr		  = m_swapChain->Present(syncInterval, 0);

	if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
	{
		// Present just surfaces the symptom. Query the device for the real cause.
		ComRef<ID3D12Device> device;
		if (!m_backBuffers.empty() && SUCCEEDED(m_backBuffers[0]->GetDevice(IID_PPV_ARGS(&device))))
		{
			HRESULT reason = device->GetDeviceRemovedReason();
			LOG_ERROR("Device removed — reason: 0x{:08X}", static_cast<unsigned>(reason));
			ThrowIfFailed(reason);
		}
	}

	ThrowIfFailed(hr);
}

void D3D12SwapChain::Resize(u32 width, u32 height)
{
	DYNAMIC_ASSERT(m_swapChain, "D3D12SwapChain::Resize: swap chain not initialized");

	// Release all back-buffer and RTV references before resizing.
	m_rtvHeap.Reset();
	for (ComRef<ID3D12Resource>& buf : m_backBuffers)
	{
		buf.Reset();
	}

	ThrowIfFailed(m_swapChain->ResizeBuffers(m_bufferCount, width, height, m_format, 0));

	m_width	 = width;
	m_height = height;

	// Re-acquire back-buffer pointers and reset state tracking.
	for (u32 i = 0; i < m_bufferCount; ++i)
	{
		ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i])));
		m_backBufferStates[i] = D3D12_RESOURCE_STATE_PRESENT;
	}

	// Retrieve the device from the first back buffer to recreate the heap.
	ComRef<ID3D12Device> device;
	ThrowIfFailed(m_backBuffers[0]->GetDevice(IID_PPV_ARGS(&device)));
	CreateRTVs(device.Get());
}

void* D3D12SwapChain::GetCurrentBackBuffer() const
{
	DYNAMIC_ASSERT(m_swapChain, "D3D12SwapChain::GetCurrentBackBuffer: not initialized");
	u32 index = m_swapChain->GetCurrentBackBufferIndex();
	return m_backBuffers[index].Get();
}

DescriptorHandle D3D12SwapChain::GetCurrentRTV() const
{
	DYNAMIC_ASSERT(m_rtvHeap, "D3D12SwapChain::GetCurrentRTV: RTV heap not initialized");
	u32 index = m_swapChain->GetCurrentBackBufferIndex();

	D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += static_cast<SIZE_T>(index) * m_rtvDescriptorSize;

	return DescriptorHandle{ static_cast<u64>(handle.ptr) };
}

void D3D12SwapChain::TransitionToRenderTarget(CommandList& cmd)
{
	u32 index				 = m_swapChain->GetCurrentBackBufferIndex();
	D3D12CommandList& d3dCmd = static_cast<D3D12CommandList&>(cmd);
	d3dCmd.TransitionResource(m_backBuffers[index].Get(), m_backBufferStates[index],
							  D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_backBufferStates[index] = D3D12_RESOURCE_STATE_RENDER_TARGET;
}

void D3D12SwapChain::TransitionToPresent(CommandList& cmd)
{
	u32 index				 = m_swapChain->GetCurrentBackBufferIndex();
	D3D12CommandList& d3dCmd = static_cast<D3D12CommandList&>(cmd);
	d3dCmd.TransitionResource(m_backBuffers[index].Get(), m_backBufferStates[index], D3D12_RESOURCE_STATE_PRESENT);
	m_backBufferStates[index] = D3D12_RESOURCE_STATE_PRESENT;
}

SwapChainFormat D3D12SwapChain::GetFormat() const
{
	return m_engineFormat;
}

u32 D3D12SwapChain::GetWidth() const
{
	return m_width;
}

u32 D3D12SwapChain::GetHeight() const
{
	return m_height;
}

void D3D12SwapChain::Cleanup()
{
	m_rtvHeap.Reset();
	for (ComRef<ID3D12Resource>& buf : m_backBuffers)
	{
		buf.Reset();
	}

	m_backBuffers.clear();
	m_backBufferStates.clear();
	m_swapChain.Reset();
}

u32 D3D12SwapChain::GetCurrentBackBufferIndex() const
{
	DYNAMIC_ASSERT(m_swapChain, "D3D12SwapChain: not initialized");
	return m_swapChain->GetCurrentBackBufferIndex();
}

#endif
