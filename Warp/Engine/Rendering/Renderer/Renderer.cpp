#include <Core/Console/ConsoleCommand.h>
#include <Debugging/Assert.h>
#include <Debugging/Logging.h>
#include <Rendering/Renderer/FrameBlackboard.h>
#include <Rendering/Renderer/FrameView.h>
#include <Rendering/Renderer/Renderer.h>
#include <Rendering/Resource/ResourceManager.h>
#include <Rendering/Window/Window.h>
#include <UI/ImGuiBackend.h>
#include <algorithm>

// Reads the Shaders folder next to the exe, which the CopyShaders target fills.
static ConsoleCommand CmdReloadShaders("r.ReloadShaders",
									   "Recompile every shader and rebuild every pipeline at the next frame");

Renderer::~Renderer() = default;

void Renderer::Init(IWindow* window, URef<Device> device)
{
	DYNAMIC_ASSERT(window, "Renderer::Init: window is null");
	DYNAMIC_ASSERT(device, "Renderer::Init: device is null");

	m_device = std::move(device);

	// Command queues
	m_graphicsQueue = m_device->CreateCommandQueue(CommandQueueType::Graphics);
	m_computeQueue	= m_device->CreateCommandQueue(CommandQueueType::Compute);
	m_copyQueue		= m_device->CreateCommandQueue(CommandQueueType::Copy);

	// Swap chain
	SwapChainDesc scDesc;
	scDesc.Window	   = window;
	scDesc.Width	   = static_cast<u32>(window->GetWidth());
	scDesc.Height	   = static_cast<u32>(window->GetHeight());
	scDesc.BufferCount = k_backBufferCount;
	scDesc.Format	   = SwapChainFormat::BGRA8;
	scDesc.bUseVsync   = false;
	m_swapChain		   = m_device->CreateSwapChain(scDesc);

	// Up front, so a shader that fails to compile stops startup instead of the
	// first frame that draws with it.
	m_pipelines = std::make_unique<PipelineLibrary>(m_device.get());
	m_gpuScene.Init(m_device.get(), *m_pipelines, k_framesInFlight);

	PassInitContext passInit;
	passInit.device	   = m_device.get();
	passInit.pipelines = m_pipelines.get();
	passInit.width	   = scDesc.Width;
	passInit.height	   = scDesc.Height;
	for (RenderPass* pass : GetPasses())
	{
		pass->Init(passInit);
	}

	// Command lists
	m_graphicsLists.push_back(m_device->CreateCommandList(CommandQueueType::Graphics, k_framesInFlight));
	m_computeLists.push_back(m_device->CreateCommandList(CommandQueueType::Compute, k_framesInFlight));
	m_copyList		 = m_device->CreateCommandList(CommandQueueType::Copy, k_framesInFlight);
	m_urgentCopyList = m_device->CreateCommandList(CommandQueueType::Copy, k_framesInFlight);

	// Upload buffer
	m_uploadBuffer = m_device->CreateUploadBuffer(k_uploadHeapSize, k_framesInFlight);

	// Worker thread pool
	m_workerPool = std::make_unique<ThreadPool>(8);

	m_reloadShadersDelegate =
		std::make_unique<MemberFuncType<Renderer, const ConsoleArgs&>>(this, &Renderer::OnReloadShaders);
	CmdReloadShaders.Subscribe(m_reloadShadersDelegate.get());

	LOG_DEBUG("Renderer initialized ({})", m_device->GetAPIName());
}

void Renderer::OnReloadShaders(const ConsoleArgs& /*args*/)
{
	m_bReloadShadersRequested = true;
}

void Renderer::SetGPUCulling(bool bEnabled)
{
	m_gpuScene.SetGPUCulling(bEnabled);
}

bool Renderer::IsGPUCulling() const
{
	return m_gpuScene.IsGPUCulling();
}

Renderer::CullStats Renderer::GetCullStats() const
{
	const GPUScene::Stats& scene = m_gpuScene.GetStats();
	return { scene.tested, scene.culled };
}

Renderer::DrawStats Renderer::GetDrawStats() const
{
	const GPUScene::Stats& scene = m_gpuScene.GetStats();

	DrawStats stats;
	stats.batches		   = scene.batches;
	stats.drawCalls		   = m_drawCalls;
	stats.numTris		   = scene.numTris;
	stats.visibleInstances = scene.visibleInstances;
	stats.dirtyTransforms  = scene.dirtyTransforms;
	stats.dirtyMeshes	   = scene.dirtyMeshes;
	stats.removedMeshes	   = scene.removedMeshes;
	stats.uploadedSlots	   = scene.uploadedSlots;
	return stats;
}

void Renderer::WaitForGPUIdle()
{
	// Null before Init, so Shutdown is safe on a renderer that never started.
	for (CommandQueue* queue : { m_graphicsQueue.get(), m_computeQueue.get(), m_copyQueue.get() })
	{
		if (queue)
		{
			queue->WaitForIdle();
		}
	}
}

void Renderer::Shutdown()
{
	WaitForGPUIdle();

	m_gpuScene.Shutdown();

	if (m_reloadShadersDelegate)
	{
		CmdReloadShaders.Unsubscribe(m_reloadShadersDelegate.get());
	}

	ShutdownImGui();

	if (m_swapChain)
	{
		m_swapChain->Cleanup();
	}

	LOG_DEBUG("Renderer shut down");
}

// ---------------------------------------------------------------------------
// ImGui integration — delegates to the API's ImGuiBackend.
// ---------------------------------------------------------------------------

void Renderer::InitImGui(IWindow* window, URef<ImGuiBackend> backend)
{
	DYNAMIC_ASSERT(backend, "Renderer::InitImGui: backend is null");

	m_imguiBackend	   = std::move(backend);
	m_imguiInitialized = m_imguiBackend->Init(window, m_device.get(), m_graphicsQueue.get(), k_framesInFlight);
	if (m_imguiInitialized)
	{
		LOG_DEBUG("ImGui initialized");
	}
	else
	{
		LOG_ERROR("Failed to initialize ImGui");
	}
}

void Renderer::ShutdownImGui()
{
	if (m_imguiBackend)
	{
		m_imguiBackend->Shutdown();
		m_imguiBackend.reset();
		m_imguiInitialized = false;
	}
}

void Renderer::NewFrameImGui()
{
	if (m_imguiBackend)
	{
		m_imguiBackend->NewFrame();
	}
}

bool Renderer::IsImGuiInitialized() const
{
	return m_imguiInitialized;
}

void Renderer::OnResize(u32 width, u32 height)
{
	if (width == 0 || height == 0)
	{
		return;
	}

	// Passes release their size dependent textures, and the GPU may still be
	// reading them from a frame in flight.
	WaitForGPUIdle();

	m_swapChain->Resize(width, height);

	for (RenderPass* pass : GetPasses())
	{
		pass->OnResize(width, height);
	}

	LOG_DEBUG("Renderer resized: {}x{}", width, height);
}

Array<RenderPass*, 5> Renderer::GetPasses()
{
	return { &m_shadowPass, &m_gbufferPass, &m_lightingPass, &m_tonemapPass, &m_imguiPass };
}

void Renderer::BeginFrame()
{
	// Requested from the console during last frame. Every frame in flight may still
	// reference the old PSOs, so the GPU has to be idle before they are destroyed.
	if (m_bReloadShadersRequested)
	{
		m_bReloadShadersRequested = false;
		WaitForGPUIdle();
		m_pipelines->ReloadAll();
	}

	// Wait for the GPU to finish with the oldest in-flight frame before reusing its slot.
	const FrameSyncPoint& retiring = m_frameSyncPoints[m_frameIndex];
	if (retiring.graphicsFenceValue > 0)
	{
		m_graphicsQueue->WaitForValue(retiring.graphicsFenceValue);
	}
	if (retiring.computeFenceValue > 0)
	{
		m_computeQueue->WaitForValue(retiring.computeFenceValue);
	}
	if (retiring.copyFenceValue > 0)
	{
		m_copyQueue->WaitForValue(retiring.copyFenceValue);
	}

	// Free staging buffers whose copies have completed on the GPU.
	{
		u64 completedCopyValue = m_copyQueue->GetCompletedValue();
		m_inFlightStagingBuffers.erase(std::remove_if(m_inFlightStagingBuffers.begin(), m_inFlightStagingBuffers.end(),
													  [completedCopyValue](const InFlightStaging& s)
													  { return s.fenceValue <= completedCopyValue; }),
									   m_inFlightStagingBuffers.end());

		m_inFlightTextureUploads.erase(std::remove_if(m_inFlightTextureUploads.begin(), m_inFlightTextureUploads.end(),
													  [completedCopyValue](const InFlightTextureUpload& s)
													  { return s.fenceValue <= completedCopyValue; }),
									   m_inFlightTextureUploads.end());
	}

	// Retire the oldest frame's upload-buffer slice so the ring buffer can reuse it.
	m_uploadBuffer->OnBeginFrame();

	// Safe now that this slot's fence has been waited on above.
	m_gpuScene.BeginFrame(m_frameIndex);

	// Reset all CPU-side per-frame allocations.
	m_frameArena.Reset();

	// Reset cross-queue wait flags for the new frame.
	m_graphicsWaitOnCopy = false;
	m_computeWaitOnCopy	 = false;

	// Open all command lists for this frame slot.
	for (URef<CommandList>& list : m_graphicsLists)
	{
		list->Begin(m_frameIndex);
	}

	for (URef<CommandList>& list : m_computeLists)
	{
		list->Begin(m_frameIndex);
	}

	m_copyList->Begin(m_frameIndex);
	m_urgentCopyList->Begin(m_frameIndex);

	// Process ResourceManager: check async loads, create GPU buffers, drain uploads.
	if (m_resourceManager)
	{
		m_resourceManager->ProcessPendingUploads(m_copyQueue->GetCompletedValue());

		for (PendingStagingUpload& upload : m_resourceManager->DrainStagingUploads())
		{
			QueueStagingUpload(upload);
		}

		for (PendingTextureUpload& upload : m_resourceManager->DrainTextureUploads())
		{
			QueueTextureUpload(upload);
		}

		// Issue CopyDest -> ShaderResource barriers for textures whose uploads completed.
		CommandList& graphicsCmd = *m_graphicsLists[0];
		for (Texture* tex : m_resourceManager->DrainTextureBarriers())
		{
			graphicsCmd.TransitionTexture(tex, ResourceState::ShaderResource);
		}
	}
}

void Renderer::SetWorld(World* world)
{
	m_world = world;
	m_gpuScene.SetWorld(world);
}

void Renderer::Draw()
{
	switch (m_renderPath)
	{
		case RenderPath::Deferred:
			DrawDeferred();
			break;
		case RenderPath::ForwardPlus:
			DrawForwardPlus();
			break;
	}
}

void Renderer::EndFrame()
{
	// Close all command lists.
	for (URef<CommandList>& list : m_graphicsLists)
	{
		list->End();
	}
	for (URef<CommandList>& list : m_computeLists)
	{
		list->End();
	}

	u64 lastCopyFenceValue = 0;

	// --- Urgent copies: submitted first, cross-queue wait inserted so
	//     graphics/compute won't execute until these specific copies finish.
	if (!m_urgentUploads.empty())
	{
		for (PendingStagingUpload& upload : m_urgentUploads)
		{
			m_urgentCopyList->CopyBuffer(upload.stagingBuffer.get(), upload.destination, 0, 0, upload.size);
		}

		m_urgentCopyList->End();

		u64 urgentFenceValue = m_copyQueue->Submit(*m_urgentCopyList);

		if (m_graphicsWaitOnCopy)
		{
			m_graphicsQueue->WaitForQueue(*m_copyQueue, urgentFenceValue);
		}
		if (m_computeWaitOnCopy)
		{
			m_computeQueue->WaitForQueue(*m_copyQueue, urgentFenceValue);
		}

		for (PendingStagingUpload& upload : m_urgentUploads)
		{
			InFlightStaging entry;
			entry.stagingBuffer = std::move(upload.stagingBuffer);
			entry.fenceValue	= urgentFenceValue;
			m_inFlightStagingBuffers.push_back(std::move(entry));
		}
		m_urgentUploads.clear();

		lastCopyFenceValue = urgentFenceValue;
	}
	else
	{
		m_urgentCopyList->End();
	}

	// --- Deferred copies: no cross-queue wait, available within k_framesInFlight frames.
	InFlightStaging entry;
	for (PendingStagingUpload& upload : m_deferredUploads)
	{
		m_copyList->CopyBuffer(upload.stagingBuffer.get(), upload.destination, 0, 0, upload.size);
	}

	// --- Texture uploads: copy each mip from staging buffer to the GPU texture.
	for (PendingTextureUpload& upload : m_deferredTextureUploads)
	{
		// Vulkan images start UNDEFINED. D3D12 creates them in COPY_DEST, so this
		// records nothing there.
		m_copyList->TransitionTexture(upload.destination, ResourceState::CopyDest);

		Buffer* stagingBuf = upload.stagingUploadBuffer->GetBackingBuffer();
		for (const TextureMipUpload& mip : upload.mips)
		{
			m_copyList->CopyBufferToTexture(stagingBuf, mip.srcOffset, mip.srcRowPitch, upload.destination,
											mip.mipLevel, mip.arraySlice);
		}
	}

	m_copyList->End();

	if (!m_deferredUploads.empty() || !m_deferredTextureUploads.empty())
	{
		u64 deferredFenceValue = m_copyQueue->Submit(*m_copyList);

		for (PendingStagingUpload& upload : m_deferredUploads)
		{
			entry.stagingBuffer = std::move(upload.stagingBuffer);
			entry.fenceValue	= deferredFenceValue;
			m_inFlightStagingBuffers.push_back(std::move(entry));
		}

		for (PendingTextureUpload& upload : m_deferredTextureUploads)
		{
			InFlightTextureUpload entry;
			entry.stagingBuffer = std::move(upload.stagingUploadBuffer);
			entry.fenceValue	= deferredFenceValue;
			m_inFlightTextureUploads.push_back(std::move(entry));
		}

		lastCopyFenceValue = deferredFenceValue;

		// Ties load tasks parked this frame to the fence value covering their copy.
		if (m_resourceManager)
		{
			m_resourceManager->OnCopySubmitted(deferredFenceValue);
		}
	}

	m_deferredUploads.clear();
	m_deferredTextureUploads.clear();

	// Submit worker lists as batches and track the signaled fence value per queue.
	FrameSyncPoint& sync = m_frameSyncPoints[m_frameIndex];
	sync.copyFenceValue	 = lastCopyFenceValue;

	if (!m_graphicsLists.empty())
	{
		Vector<CommandList*> graphicsBatch(m_graphicsLists.size());
		for (u32 i = 0; i < m_graphicsLists.size(); ++i)
		{
			graphicsBatch[i] = m_graphicsLists[i].get();
		}
		sync.graphicsFenceValue = m_graphicsQueue->Submit(graphicsBatch);
	}

	if (!m_computeLists.empty())
	{
		Vector<CommandList*> computeBatch(m_computeLists.size());
		for (u32 i = 0; i < m_computeLists.size(); ++i)
		{
			computeBatch[i] = m_computeLists[i].get();
		}
		sync.computeFenceValue = m_computeQueue->Submit(computeBatch);
	}

	m_swapChain->Present();
	m_frameIndex = (m_frameIndex + 1) % k_framesInFlight;
}

void Renderer::DrawDeferred()
{
	if (!m_world)
	{
		LOG_ERROR("Renderer::DrawDeferred: No valid World");
		return;
	}

	if (!m_resourceManager)
	{
		LOG_ERROR("Renderer::DrawDeferred: No valid Resource Manager, nothing to draw");
		return;
	}

	const FrameView view = BuildFrameView(*m_world, m_swapChain->GetWidth(), m_swapChain->GetHeight());

	PassContext context;
	context.pipelines	 = m_pipelines.get();
	context.uploadBuffer = m_uploadBuffer.get();
	context.swapChain	 = m_swapChain.get();
	context.imgui		 = IsImGuiInitialized() ? m_imguiBackend.get() : nullptr;

	FrameBlackboard blackboard;

	// Recorded in this order by Execute below. Each call reads what it needs from
	// the view and the blackboard, and adds its outputs for the passes after it.
	m_gpuScene.AddUploadAndCull(m_renderGraph, blackboard, view, context);
	m_shadowPass.AddToGraph(m_renderGraph, blackboard, view, context);
	m_gbufferPass.AddToGraph(m_renderGraph, blackboard, view, context);
	m_gpuScene.AddReadback(m_renderGraph);
	m_lightingPass.AddToGraph(m_renderGraph, blackboard, view, context);
	m_tonemapPass.AddToGraph(m_renderGraph, blackboard, view, context);
	m_imguiPass.AddToGraph(m_renderGraph, blackboard, view, context);

	CommandList& cmd = *m_graphicsLists[0];
	m_renderGraph.Execute(cmd);

	// Every draw so far was recorded by this frame's passes.
	m_drawCalls = cmd.GetDrawCount();

	m_swapChain->TransitionToPresent(cmd);
}

void Renderer::DrawForwardPlus()
{
	// TODO: light culling compute pass, forward opaque pass
	DrawDeferred();
}

void Renderer::QueueStagingUpload(PendingStagingUpload& upload)
{
	DYNAMIC_ASSERT(upload.IsValid(), "Renderer::QueueStagingUpload: invalid upload");
	m_deferredUploads.push_back(std::move(upload));
}

void Renderer::QueueTextureUpload(PendingTextureUpload& upload)
{
	DYNAMIC_ASSERT(upload.IsValid(), "Renderer::QueueTextureUpload: invalid upload");
	m_deferredTextureUploads.push_back(std::move(upload));
}

void Renderer::QueueCopyForThisFrame(PendingStagingUpload& upload, CommandQueueType queueType)
{
	DYNAMIC_ASSERT(upload.IsValid(), "Renderer::QueueCopyForThisFrame: invalid upload");

	m_urgentUploads.push_back(std::move(upload));

	switch (queueType)
	{
		case CommandQueueType::Graphics:
			m_graphicsWaitOnCopy = true;
			break;
		case CommandQueueType::Compute:
			m_computeWaitOnCopy = true;
			break;
		case CommandQueueType::Copy:
			break; // No cross-queue wait needed
	}
}
