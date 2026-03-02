#ifdef WARP_BUILD_DX12

#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12Renderer.h>
#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12Device.h>
#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12Texture.h>
#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12SwapChain.h>
#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12CommandList.h>
#include <Rendering/Renderer/Device.h>
#include <Rendering/Window/Window.h>
#include <Debugging/Assert.h>
#include <Debugging/Logging.h>

void D3D12Renderer::Init(IWindow* window)
{
	DYNAMIC_ASSERT(window, "D3D12Renderer::Init: window is null");

	// -------------------------------------------------------------------------
	// Device
	// -------------------------------------------------------------------------
	DeviceDesc deviceDesc;
#if defined(WARP_DEBUG)
	deviceDesc.bEnableDebugLayer = true;
#endif
	auto d3dDevice = std::make_unique<D3D12Device>();
	d3dDevice->Initialize(deviceDesc);
	m_device = std::move(d3dDevice);

	// -------------------------------------------------------------------------
	// Command queues
	// -------------------------------------------------------------------------
	m_graphicsQueue = m_device->CreateCommandQueue(CommandQueueType::Graphics);
	m_computeQueue  = m_device->CreateCommandQueue(CommandQueueType::Compute);
	m_copyQueue     = m_device->CreateCommandQueue(CommandQueueType::Copy);

	// -------------------------------------------------------------------------
	// Swap chain
	// -------------------------------------------------------------------------
	SwapChainDesc scDesc;
	scDesc.Window      = window;
	scDesc.Width       = static_cast<u32>(window->GetWidth());
	scDesc.Height      = static_cast<u32>(window->GetHeight());
	scDesc.BufferCount = k_backBufferCount;
	scDesc.Format      = SwapChainFormat::BGRA8;
	scDesc.bUseVsync   = false;
	m_swapChain = m_device->CreateSwapChain(scDesc);

	// -------------------------------------------------------------------------
	// Depth buffer
	// -------------------------------------------------------------------------
	CreateDepthBuffer(scDesc.Width, scDesc.Height);

	// -------------------------------------------------------------------------
	// Command lists  (one graphics + one compute worker, one copy list)
	// -------------------------------------------------------------------------
	m_graphicsLists.push_back(
		m_device->CreateCommandList(CommandQueueType::Graphics, k_framesInFlight));
	m_computeLists.push_back(
		m_device->CreateCommandList(CommandQueueType::Compute, k_framesInFlight));
	m_copyList = m_device->CreateCommandList(CommandQueueType::Copy, k_framesInFlight);

	// -------------------------------------------------------------------------
	// Upload buffer (GPU-visible ring buffer for per-frame constant data)
	// -------------------------------------------------------------------------
	m_uploadBuffer = m_device->CreateUploadBuffer(k_uploadHeapSize, k_framesInFlight);

	// -------------------------------------------------------------------------
	// Shader-visible descriptor heap (CBV/SRV/UAV)
	// -------------------------------------------------------------------------
	// 4096 descriptors gives ample room for per-frame dynamic SRVs and CBVs.
	m_srvHeap.Initialize(d3dDevice->GetNativeDevice(), 4096);

	// Wire the device + heap into every graphics command list so they can bind
	// descriptors and create inline SRVs without knowing about D3D12Device.
	for (auto& list : m_graphicsLists)
	{
		D3D12CommandList* d3dList = static_cast<D3D12CommandList*>(list.get());
		d3dList->SetDevice(d3dDevice->GetNativeDevice());
		d3dList->SetSRVHeap(&m_srvHeap);
	}

	// -------------------------------------------------------------------------
	// Test triangle PSO + shaders
	// -------------------------------------------------------------------------
	CreateTestTriangle();

	// -------------------------------------------------------------------------
	// Worker thread pool
	// -------------------------------------------------------------------------
	m_workerPool = std::make_unique<ThreadPool>(1);

	LOG_DEBUG("D3D12Renderer initialized");
}

void D3D12Renderer::CreateDepthBuffer(u32 width, u32 height)
{
	D3D12Device* d3dDevice = static_cast<D3D12Device*>(m_device.get());

	// Create the depth texture via the abstract device.
	TextureDesc depthDesc;
	depthDesc.type   = TextureType::Texture2D;
	depthDesc.width  = width;
	depthDesc.height = height;
	depthDesc.format = TextureFormat::Depth32F;
	depthDesc.usage  = TextureUsage::DepthStencil;
	m_depthTexture = m_device->CreateTexture(depthDesc);

	// Create a single-slot DSV descriptor heap.
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	heapDesc.NumDescriptors = 1;
	heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(d3dDevice->GetNativeDevice()->CreateDescriptorHeap(
		&heapDesc, IID_PPV_ARGS(&m_dsvHeap)));

	// Create the DSV — D32_FLOAT view into the Depth32F texture.
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format        = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags         = D3D12_DSV_FLAG_NONE;

	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
	d3dDevice->GetNativeDevice()->CreateDepthStencilView(
		static_cast<D3D12Texture*>(m_depthTexture.get())->GetNativeResource(),
		&dsvDesc, dsvHandle);

	m_depthHandle = DescriptorHandle{ static_cast<u64>(dsvHandle.ptr) };
}

void D3D12Renderer::Shutdown()
{
	// Drain all queues before releasing GPU resources.
	if (m_graphicsQueue) m_graphicsQueue->Reset();
	if (m_computeQueue)  m_computeQueue->Reset();
	if (m_copyQueue)     m_copyQueue->Reset();

	m_testTriPSO.reset();
	m_testTriVS.reset();
	m_testTriPS.reset();

	m_dsvHeap.Reset();
	m_depthTexture.reset();
	m_depthHandle = {};

	if (m_swapChain) m_swapChain->Cleanup();

	LOG_DEBUG("D3D12Renderer shut down");
}

void D3D12Renderer::OnResize(u32 width, u32 height)
{
	if (width == 0 || height == 0)
		return;

	// Drain queues so no GPU work references the old resources.
	if (m_graphicsQueue) m_graphicsQueue->Reset();
	if (m_computeQueue)  m_computeQueue->Reset();
	if (m_copyQueue)     m_copyQueue->Reset();

	// Resize the swap chain back buffers.
	m_swapChain->Resize(width, height);

	// Recreate the depth buffer at the new resolution.
	m_dsvHeap.Reset();
	m_depthTexture.reset();
	m_depthHandle = {};
	CreateDepthBuffer(width, height);

	LOG_DEBUG("D3D12Renderer resized: {}x{}", width, height);
}

#endif
