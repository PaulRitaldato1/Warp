#ifdef WARP_BUILD_DX12

#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12Renderer.h>
#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12Device.h>
#include <Rendering/Window/Window.h>
#include <Debugging/Assert.h>
#include <Debugging/Logging.h>

void D3D12Renderer::Init(IWindow* window)
{
	DYNAMIC_ASSERT(window, "D3D12Renderer::Init: window is null");

	// --- Device ---
	m_device = std::make_unique<D3D12Device>();
	DeviceDesc deviceDesc;
#ifdef _DEBUG
	deviceDesc.bEnableDebugLayer = true;
#endif
	m_device->Initialize(deviceDesc);

	// --- Command queues ---
	m_graphicsQueue = m_device->CreateCommandQueue(CommandQueueType::Graphics);
	m_computeQueue  = m_device->CreateCommandQueue(CommandQueueType::Compute);
	m_copyQueue     = m_device->CreateCommandQueue(CommandQueueType::Copy);

	// --- Worker thread pool + per-worker command lists ---
	const u32 workerCount = std::max(1u, static_cast<u32>(std::thread::hardware_concurrency()) - 1u);
	m_workerPool = std::make_unique<ThreadPool>(workerCount);

	m_graphicsLists.resize(workerCount);
	m_computeLists.resize(workerCount);
	for (u32 i = 0; i < workerCount; ++i)
	{
		m_graphicsLists[i] = m_device->CreateCommandList(CommandQueueType::Graphics, k_framesInFlight);
		m_computeLists[i]  = m_device->CreateCommandList(CommandQueueType::Compute,  k_framesInFlight);
	}
	m_copyList = m_device->CreateCommandList(CommandQueueType::Copy, k_framesInFlight);
	m_urgentCopyList = m_device->CreateCommandList(CommandQueueType::Copy, k_framesInFlight);
	
	// --- Swap chain ---
	SwapChainDesc scDesc;
	scDesc.Window      = window;
	scDesc.Width       = static_cast<u32>(window->GetWidth());
	scDesc.Height      = static_cast<u32>(window->GetHeight());
	scDesc.BufferCount = k_framesInFlight;
	scDesc.Format      = SwapChainFormat::BGRA8;
	scDesc.bUseVsync   = false;
	m_swapChain = m_device->CreateSwapChain(scDesc);

	// --- Upload buffer ---
	m_uploadBuffer = m_device->CreateUploadBuffer(k_uploadHeapSize, k_framesInFlight);

	LOG_DEBUG("D3D12Renderer initialized ({} workers)", workerCount);
}

void D3D12Renderer::Shutdown()
{
	// Drain all in-flight work before releasing resources.
	if (m_graphicsQueue) { m_graphicsQueue->Reset(); }
	if (m_computeQueue)  { m_computeQueue->Reset(); }
	if (m_copyQueue)     { m_copyQueue->Reset(); }

	if (m_uploadBuffer)  { m_uploadBuffer->Cleanup(); }
	if (m_swapChain)     { m_swapChain->Cleanup(); }

	m_workerPool.reset();
	m_copyList.reset();
	m_computeLists.clear();
	m_graphicsLists.clear();
	m_copyQueue.reset();
	m_computeQueue.reset();
	m_graphicsQueue.reset();
	m_uploadBuffer.reset();
	m_swapChain.reset();
	m_device.reset();

	LOG_DEBUG("D3D12Renderer shutdown complete");
}

void D3D12Renderer::OnResize(u32 width, u32 height)
{
	// Drain the GPU before resizing swap chain buffers.
	if (m_graphicsQueue) { m_graphicsQueue->Reset(); }

	if (m_swapChain) { m_swapChain->Resize(width, height); }
}

#endif
