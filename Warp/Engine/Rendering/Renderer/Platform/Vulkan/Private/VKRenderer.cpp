#include "Renderer/CommandQueue.h"
#ifdef WARP_BUILD_VK

#include <Rendering/Renderer/Platform/Vulkan/VKRenderer.h>
#include <Rendering/Renderer/Platform/Vulkan/VKDevice.h>
#include <Rendering/Renderer/Platform/Vulkan/VKTexture.h>
#include <Rendering/Window/Window.h>
#include <Debugging/Assert.h>
#include <Debugging/Logging.h>

// ---------------------------------------------------------------------------
// VKRenderer
// ---------------------------------------------------------------------------

void VKRenderer::Init(IWindow* window)
{
	DYNAMIC_ASSERT(window, "VKRenderer::Init: window is null");

	// -------------------------------------------------------------------------
	// Device
	// -------------------------------------------------------------------------

	DeviceDesc deviceDesc;
#if defined(WARP_DEBUG)
	deviceDesc.bEnableDebugLayer = true;
#endif

	auto vkDevice = std::make_unique<VKDevice>();
	vkDevice->Initialize(deviceDesc);
	m_device = std::move(vkDevice);

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
	// Command lists
	// -------------------------------------------------------------------------

	m_graphicsLists.push_back(
		m_device->CreateCommandList(CommandQueueType::Graphics, k_framesInFlight));
	m_computeLists.push_back(
		m_device->CreateCommandList(CommandQueueType::Compute, k_framesInFlight));
	m_copyList = m_device->CreateCommandList(CommandQueueType::Copy, k_framesInFlight);
	m_urgentCopyList = m_device->CreateCommandList(CommandQueueType::Copy, k_framesInFlight);
	// -------------------------------------------------------------------------
	// Upload buffer
	// -------------------------------------------------------------------------

	m_uploadBuffer = m_device->CreateUploadBuffer(k_uploadHeapSize, k_framesInFlight);

	// -------------------------------------------------------------------------
	// Test triangle PSO
	// -------------------------------------------------------------------------

	CreateTestTriangle();

	// -------------------------------------------------------------------------
	// Worker thread pool
	// -------------------------------------------------------------------------

	m_workerPool = std::make_unique<ThreadPool>(1);

	LOG_DEBUG("VKRenderer initialized");
}

void VKRenderer::CreateDepthBuffer(u32 width, u32 height)
{
	TextureDesc depthDesc;
	depthDesc.type   = TextureType::Texture2D;
	depthDesc.width  = width;
	depthDesc.height = height;
	depthDesc.format = TextureFormat::Depth32F;
	depthDesc.usage  = TextureUsage::DepthStencil;
	m_depthTexture = m_device->CreateTexture(depthDesc);

	// Store the image view as the depth descriptor handle.
	VKTexture* vkDepth = static_cast<VKTexture*>(m_depthTexture.get());
	m_depthHandle = vkDepth->GetDescriptorHandle();
}

void VKRenderer::Shutdown()
{
	if (m_device)
	{
		m_device->WaitForIdle();
	}

	m_testTriPSO.reset();
	m_testTriVS.reset();
	m_testTriPS.reset();

	m_depthTexture.reset();
	m_depthHandle = {};

	if (m_swapChain)
	{
		m_swapChain->Cleanup();
	}

	LOG_DEBUG("VKRenderer shut down");
}

void VKRenderer::OnResize(u32 width, u32 height)
{
	if (width == 0 || height == 0)
	{
		return;
	}

	if (m_device)
	{
		m_device->WaitForIdle();
	}

	m_swapChain->Resize(width, height);

	m_depthTexture.reset();
	m_depthHandle = {};
	CreateDepthBuffer(width, height);

	LOG_DEBUG("VKRenderer resized: {}x{}", width, height);
}

#endif // WARP_BUILD_VK
