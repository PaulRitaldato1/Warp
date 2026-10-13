#include "DirectXMath.h"
#include "Math/Frustum.h"
#include "Math/Math.h"
#include <Core/Console/ConsoleCommand.h>
#include <Core/Console/ConsoleVariable.h>
#include <Core/ECS/Components/CameraComponent.h>
#include <Core/ECS/Components/LightComponent.h>
#include <Core/ECS/Components/MeshComponent.h>
#include <Core/ECS/Components/SkyLightComponent.h>
#include <Core/ECS/Components/TransformComponent.h>
#include <Core/ECS/World.h>
#include <Debugging/Assert.h>
#include <Debugging/Logging.h>
#include <Debugging/Profiler.h>
#include <Rendering/Lighting/LightData.h>
#include <Rendering/Mesh/Mesh.h>
#include <Rendering/Passes/CullPass.h>
#include <Rendering/Passes/GBufferPass.h>
#include <Rendering/Passes/ImGuiPass.h>
#include <Rendering/Passes/LightingPass.h>
#include <Rendering/Passes/ShadowPass.h>
#include <Rendering/Passes/TonemapPass.h>
#include <Rendering/Renderer/DescriptorHandle.h>
#include <Rendering/Renderer/Pipeline.h>
#include <Rendering/Renderer/Renderer.h>
#include <Rendering/Renderer/ResourceState.h>
#include <Rendering/Renderer/Texture.h>
#include <Rendering/Renderer/UploadBuffer.h>
#include <Rendering/Resource/MeshResource.h>
#include <Rendering/Resource/ResourceManager.h>
#include <Rendering/Resource/TextureResource.h>
#include <Rendering/Window/Window.h>
#include <UI/ImGuiBackend.h>
#include <algorithm>
#include <ranges>

// Static: only this file reads it. Other code goes through Renderer::SetGPUCulling,
// or through the registry by name, which is how the console will reach it.
static Cvar<bool> CvarGPUCulling("r.GPUCulling", true,
								 "Cull instances on the GPU. Off uses the CPU cull, kept as a reference");

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

	// Depth buffer
	CreateDepthBuffer(scDesc.Width, scDesc.Height);

	// Up front, so a shader that fails to compile stops startup instead of the
	// first frame that draws with it.
	m_pipelines = std::make_unique<PipelineLibrary>(m_device.get());
	CreatePipelines();

	// Size independent, unlike the GBuffer, so made once here.
	InitShadowTextures();

	// Command lists
	m_graphicsLists.push_back(m_device->CreateCommandList(CommandQueueType::Graphics, k_framesInFlight));
	m_computeLists.push_back(m_device->CreateCommandList(CommandQueueType::Compute, k_framesInFlight));
	m_copyList		 = m_device->CreateCommandList(CommandQueueType::Copy, k_framesInFlight);
	m_urgentCopyList = m_device->CreateCommandList(CommandQueueType::Copy, k_framesInFlight);

	// Upload buffer
	m_uploadBuffer = m_device->CreateUploadBuffer(k_uploadHeapSize, k_framesInFlight);

	// Worker thread pool
	m_workerPool = std::make_unique<ThreadPool>(8);

	// Owned here, unsubscribed in Shutdown: the cvar outlives the renderer.
	m_gpuCullingChangedDelegate =
		std::make_unique<MemberFuncType<Renderer, bool>>(this, &Renderer::OnGPUCullingChanged);
	CvarGPUCulling.SubscribeToChanged(m_gpuCullingChangedDelegate.get());

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
	CvarGPUCulling.Set(bEnabled);
}

bool Renderer::IsGPUCulling() const
{
	return CvarGPUCulling.Get();
}

void Renderer::OnGPUCullingChanged(bool bEnabled)
{
	// Nothing to rebuild: the next frame picks a path from the cvar. This is where
	// a cvar that needs work on change, like vsync, would do it.
	LOG_DEBUG("Renderer: r.GPUCulling = {}, culling on the {}", bEnabled, bEnabled ? "GPU" : "CPU");
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

	if (m_gpuCullingChangedDelegate)
	{
		CvarGPUCulling.UnsubscribeFromChanged(m_gpuCullingChangedDelegate.get());
	}

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

	// The depth and GBuffer textures are released below, and the GPU may still be
	// reading them from a frame in flight.
	WaitForGPUIdle();

	m_swapChain->Resize(width, height);

	m_depthTexture.reset();
	CreateDepthBuffer(width, height);

	m_gbufferSimple.albedo.reset();
	m_gbufferSimple.normal.reset();
	m_gbufferSimple.material.reset();
	m_gbufferSimple.emissive.reset();
	m_hdrTarget.reset();

	LOG_DEBUG("Renderer resized: {}x{}", width, height);
}

void Renderer::CreateDepthBuffer(u32 width, u32 height)
{
	TextureDesc depthDesc;
	depthDesc.type	 = TextureType::Texture2D;
	depthDesc.width	 = width;
	depthDesc.height = height;
	depthDesc.format = TextureFormat::Depth32F;
	depthDesc.usage	 = TextureUsage::DepthStencilSampled;
	m_depthTexture	 = m_device->CreateTexture(depthDesc);
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
	m_retiredBuffers[m_frameIndex].clear();
	m_readbackStats = ReadArgsReadback();

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

	m_drawList.Clear();
}

void Renderer::SetWorld(World* world)
{
	m_world = world;
	m_scene.Reset();
	if (m_world)
	{
		m_world->TrackChanges<TransformComponent>();
		m_world->TrackChanges<MeshComponent>();
	}
}

void Renderer::Draw()
{
	m_drawStats					 = {};
	m_drawStats.numTris			 = m_readbackStats.numTris;
	m_drawStats.visibleInstances = m_readbackStats.visibleInstances;

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

	// Find the active camera from the ECS.
	Mat4 viewProj		= {};
	Vec3 cameraPosition = {};

	{
		bool hasCamera = false;

		m_world->Each<const TransformComponent, const CameraComponent>(
			[&](Entity entity, const TransformComponent& transform, const CameraComponent& camera)
			{
				if (camera.isActive)
				{
					viewProj	   = camera.viewProj;
					cameraPosition = transform.position;
					hasCamera	   = true;
				}
			});

		if (!hasCamera)
		{
			LOG_ERROR("Renderer::DrawDeferred: No valid Camera in the world");
		}
	}

	const Array<Vec4, 6> cameraFrustum = ExtractFrustumPlanes(viewProj);

	// Reset by OnResize, so recreated here at the new size.
	if (!m_gbufferSimple.IsInitialized())
	{
		InitGBufferTextures();
	}

	// ---------------------------------------------------------------------------
	// Sky light is gathered first: its view-projection is needed to cull shadow
	// casters during the mesh gather below.
	// ---------------------------------------------------------------------------

	SkyParameters skyParameters{};
	Vector<LightInfo> skyLightInfos;

	m_world->Each<const TransformComponent, const SkyLightComponent>(
		[&](Entity entity, const TransformComponent& transform, const SkyLightComponent& skyComp)
		{
			if (skyParameters.brightness)
			{
				return; // use the first one
			}

			skyParameters.skyColorZenith   = skyComp.skyColorZenith;
			skyParameters.skyColorHorizon  = skyComp.skyColorHorizon;
			skyParameters.horizonSharpness = skyComp.horizonSharpness;
			skyParameters.groundColor	   = skyComp.groundColor;
			skyParameters.groundFade	   = skyComp.groundFade;
			skyParameters.sunColor		   = skyComp.sunColor;
			skyParameters.sunIntensity	   = skyComp.sunIntensity;
			skyParameters.sunDiscSize	   = skyComp.sunDiscSize;
			skyParameters.brightness	   = skyComp.lightIntensity;

			Vec3 sunDirection = transform.Forward();
			{
				using namespace DirectX;
				XMVECTOR sunDir = XMVector3Normalize(XMLoadFloat3(&sunDirection));
				XMStoreFloat3(&skyParameters.sunDirection, XMVectorNegate(sunDir));
			}

			LightInfo sunLight;
			sunLight.position		= {};
			sunLight.range			= 0.f;
			sunLight.color			= skyComp.sunColor;
			sunLight.intensity		= skyComp.lightIntensity;
			sunLight.direction		= sunDirection;
			sunLight.type			= 1; // Directional
			sunLight.innerConeAngle = 0.f;
			sunLight.outerConeAngle = 0.f;
			sunLight.padding		= {};
			skyLightInfos.push_back(sunLight);
		});

	// One directional shadow map, so the first sky light drives it.
	Mat4 directionalLightViewProj	= {};
	Array<Vec4, 6> shadowFrustum	= {};
	const bool hasDirectionalShadow = !skyLightInfos.empty();

	if (hasDirectionalShadow)
	{
		using namespace DirectX;

		XMVECTOR lightDir = XMVector3Normalize(XMLoadFloat3(&skyLightInfos[0].direction));
		XMVECTOR lightPos = XMVectorScale(XMVectorNegate(lightDir), 50.f);
		XMVECTOR up		  = XMVectorSet(0.f, 1.f, 0.f, 0.f);

		// Avoid degenerate up vector when light points nearly straight up or down.
		if (fabsf(XMVectorGetY(lightDir)) > 0.99f)
		{
			up = XMVectorSet(1.f, 0.f, 0.f, 0.f);
		}

		XMMATRIX lightView = XMMatrixLookToLH(lightPos, lightDir, up);

		float orthoHalfSize = 30.f;
		float nearPlane		= 0.1f;
		float farPlane		= 100.f;
		XMMATRIX lightProj	= XMMatrixOrthographicLH(orthoHalfSize * 2.f, orthoHalfSize * 2.f, nearPlane, farPlane);

		XMStoreFloat4x4(&directionalLightViewProj, XMMatrixMultiply(lightView, lightProj));

		// Gribb-Hartmann needs no special case for an orthographic matrix, which is
		// why the camera and the light share one extraction path.
		shadowFrustum = ExtractFrustumPlanes(directionalLightViewProj);
	}

	// ---------------------------------------------------------------------------
	// Sync persistent instance data from the ECS, then cull it per pass. Sync only
	// touches entities that changed; the cull walks every slot but reads only
	// cached bounds.
	// ---------------------------------------------------------------------------

	const RenderScene::SyncStats syncStats = m_scene.Sync(*m_world, *m_resourceManager);
	m_drawStats.dirtyTransforms			   = syncStats.dirtyTransforms;
	m_drawStats.dirtyMeshes				   = syncStats.dirtyMeshes;
	m_drawStats.removedMeshes			   = syncStats.removed;

	// Shadow casters are tested against the light instead, since geometry outside
	// the view can still cast into it.
	const u32 cameraFlags = RenderFlags_Visible;
	const u32 shadowFlags = static_cast<u32>(RenderFlags_Visible | RenderFlags_CastShadow);

	// Both culls end up in the same shape: args indexed by batch, and per batch
	// an offset and count into a visible index list.
	const RenderScene::CullRegions regions = m_scene.ComputeCullRegions();
	Vector<DrawIndexedArgs> cameraArgs	   = BuildDrawArgs();
	Vector<DrawIndexedArgs> shadowArgs	   = cameraArgs;

	// Read once: the cull path, the args buffer state and the dispatch below must
	// agree, even if the console changes the cvar partway through the frame.
	const bool bGPUCulling = CvarGPUCulling.Get();

	if (bGPUCulling)
	{
		// The CPU no longer knows which batches have survivors, so every batch with
		// members is drawn. One with none becomes a zero instance draw.
		m_drawList.batchItems		= BuildBatchItems(regions.occupiedRegions);
		m_drawList.shadowBatchItems = hasDirectionalShadow ? m_drawList.batchItems : Vector<BatchItem>{};
		m_cullStats					= {};
	}
	else
	{
		m_scene.Cull(cameraFrustum, cameraFlags, m_cameraCull);

		if (hasDirectionalShadow)
		{
			m_scene.Cull(shadowFrustum, shadowFlags, m_shadowCull);
		}
		else
		{
			m_shadowCull.indices.clear();
			m_shadowCull.batches.clear();
		}

		m_cullStats					= { m_cameraCull.tested, m_cameraCull.culled };
		m_drawList.batchItems		= BuildBatchItems(m_cameraCull.batches);
		m_drawList.shadowBatchItems = BuildBatchItems(m_shadowCull.batches);

		for (const RenderScene::VisibleBatch& visible : m_cameraCull.batches)
		{
			cameraArgs[visible.batchIndex].instanceCount = visible.count;
		}
		for (const RenderScene::VisibleBatch& visible : m_shadowCull.batches)
		{
			shadowArgs[visible.batchIndex].instanceCount = visible.count;
		}
	}

	LightList lightList;
	m_world->Each<const TransformComponent, const LightComponent>(
		[&](Entity entity, const TransformComponent& transform, const LightComponent& lightComp)
		{
			LightItem item;
			item.position		= transform.position;
			item.direction		= transform.Forward();
			item.color			= lightComp.color;
			item.intensity		= lightComp.intensity;
			item.range			= lightComp.range;
			item.innerConeAngle = lightComp.innerConeAngle;
			item.outerConeAngle = lightComp.outerConeAngle;
			item.type			= lightComp.type;
			item.castShadows	= lightComp.castShadows;

			u32 itemIndex = static_cast<u32>(lightList.items.size());
			lightList.items.push_back(item);

			if (lightComp.castShadows)
			{
				lightList.shadowCasters.push_back(itemIndex);
			}
		});

	m_drawStats.batches = static_cast<u32>(m_drawList.batchItems.size());

	// ---------------------------------------------------------------------------
	// Passes, recorded in this order by m_renderGraph.Execute at the end. Every
	// buffer is grown first, so the pointers passes are given stay valid while
	// the graph runs. Each pass copies its inputs, so none depend on these locals.
	// ---------------------------------------------------------------------------

	// Nothing to cull when no batch has members.
	const bool bRunGPUCull = bGPUCulling && regions.visibleListSize > 0;

	const bool bInstancesGrown = GrowSceneBuffers(regions, bGPUCulling);
	const PassContext context{ m_pipelines.get(), m_uploadBuffer.get() };

	AddSceneUploadPass(bInstancesGrown, bGPUCulling, bRunGPUCull ? regions.regionStarts : Vector<u32>{},
					   std::move(cameraArgs), std::move(shadowArgs));

	if (bRunGPUCull)
	{
		CullPassInputs cull;
		cull.pipeline	  = m_cullPipeline;
		cull.instances	  = m_instanceBuffer.get();
		cull.regionStarts = m_regionStartBuffer.get();
		cull.slotCount	  = static_cast<u32>(m_scene.GetInstances().size());
		cull.views.push_back({ cameraFrustum, cameraFlags, m_drawArgsBuffer.get(), m_slotIndexBuffer.get() });

		if (hasDirectionalShadow)
		{
			cull.views.push_back(
				{ shadowFrustum, shadowFlags, m_shadowDrawArgsBuffer.get(), m_shadowSlotIndexBuffer.get() });
		}

		AddCullPass(m_renderGraph, context, std::move(cull));
	}

	// One directional light drives the shadow map. Its matrix and frustum were built
	// before the gather so casters could be culled against it there.
	ShadowPassInputs shadow;
	shadow.pipeline		  = m_directionalShadowPipeline;
	shadow.shadowMap	  = m_shadowTextures.directionalShadowMap.get();
	shadow.resolution	  = m_shadowResolution;
	shadow.bHasLight	  = hasDirectionalShadow;
	shadow.lightViewProj  = directionalLightViewProj;
	shadow.batches		  = m_drawList.shadowBatchItems;
	shadow.instances	  = m_instanceBuffer.get();
	shadow.visibleIndices = m_shadowSlotIndexBuffer.get();
	shadow.drawArgs		  = m_shadowDrawArgsBuffer.get();
	AddShadowPass(m_renderGraph, context, shadow);

	const GBufferTargets gbufferTargets{ m_gbufferSimple.albedo.get(), m_gbufferSimple.normal.get(),
										 m_gbufferSimple.material.get(), m_gbufferSimple.emissive.get() };

	GBufferPassInputs gbuffer;
	gbuffer.pipeline	   = m_deferredGeomPipeline;
	gbuffer.targets		   = gbufferTargets;
	gbuffer.depth		   = m_depthTexture.get();
	gbuffer.viewProj	   = viewProj;
	gbuffer.batches		   = m_drawList.batchItems;
	gbuffer.instances	   = m_instanceBuffer.get();
	gbuffer.visibleIndices = m_slotIndexBuffer.get();
	gbuffer.drawArgs	   = m_drawArgsBuffer.get();
	AddGBufferPass(m_renderGraph, context, gbuffer);

	AddArgsReadbackPass(hasDirectionalShadow);

	// The sky light is still gathered from the ECS above, since it's a unique
	// component that drives both the procedural sky and a directional light.
	LightingPassInputs lighting;
	lighting.pipeline		= m_deferredLightPipeline;
	lighting.gbuffer		= gbufferTargets;
	lighting.depth			= m_depthTexture.get();
	lighting.shadowMap		= m_shadowTextures.directionalShadowMap.get();
	lighting.hdrTarget		= m_hdrTarget.get();
	lighting.viewProj		= viewProj;
	lighting.cameraPosition = cameraPosition;
	lighting.lights			= std::move(lightList);
	lighting.skyLights		= std::move(skyLightInfos);
	lighting.sky			= skyParameters;
	lighting.lightViewProj	= directionalLightViewProj;
	AddLightingPass(m_renderGraph, context, lighting);

	TonemapPassInputs tonemap;
	tonemap.pipeline  = m_tonemapPipeline;
	tonemap.hdrTarget = m_hdrTarget.get();
	tonemap.swapChain = m_swapChain.get();
	AddTonemapPass(m_renderGraph, context, tonemap);

	if (IsImGuiInitialized())
	{
		AddImGuiPass(m_renderGraph, m_imguiBackend.get(), m_swapChain.get());
	}

	CommandList& cmd = *m_graphicsLists[0];
	m_renderGraph.Execute(cmd);

	// Every draw so far was recorded by this frame's passes.
	m_drawStats.drawCalls = cmd.GetDrawCount();

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

bool Renderer::EnsureBufferCapacity(URef<Buffer>& buffer, u32& capacity, u32 needed, u32 stride, const char* name,
									bool bUnorderedAccess)
{
	if (buffer && needed <= capacity)
	{
		return false;
	}

	// Geometric growth so a steadily rising instance count does not reallocate
	// every frame.
	u32 newCapacity = capacity > 0 ? capacity : 1024;
	while (newCapacity < needed)
	{
		newCapacity *= 2;
	}

	if (buffer)
	{
		m_retiredBuffers[m_frameIndex].push_back(std::move(buffer));
	}

	BufferDesc desc;
	desc.type			  = BufferType::Structured;
	desc.numElements	  = newCapacity;
	desc.stride			  = stride;
	desc.name			  = name;
	desc.bUnorderedAccess = bUnorderedAccess;

	buffer	 = m_device->CreateBuffer(desc);
	capacity = newCapacity;

	LOG_DEBUG("Renderer: {} grown to {} elements", name, newCapacity);
	return true;
}

void Renderer::UploadInstances(CommandList& cmd, bool bFullUpload)
{
	PROFILE_SCOPE("UploadInstances");

	const Vector<InstanceData>& instances = m_scene.GetInstances();
	Vector<u32>& pending				  = m_scene.GetPendingUploads();
	const u32 slotCount					  = static_cast<u32>(instances.size());

	if (slotCount == 0)
	{
		m_scene.ClearUploads();
		return;
	}

	DYNAMIC_ASSERT(m_instanceCapacity >= slotCount, "Renderer::UploadInstances: grow it in GrowSceneBuffers first");

	// Past a quarter dirty, one big copy beats many small ones.
	const bool bCopyAll = bFullUpload || pending.size() * 4 > slotCount;
	if (!bCopyAll && pending.empty())
	{
		return;
	}

	Buffer* staging = m_uploadBuffer->GetBackingBuffer();
	cmd.TransitionBuffer(m_instanceBuffer.get(), ResourceState::CopyDest);

	if (bCopyAll)
	{
		const u64 bytes		= static_cast<u64>(slotCount) * sizeof(InstanceData);
		UploadResult staged = m_uploadBuffer->AllocAndCopy(instances.data(), bytes);
		cmd.CopyBuffer(staging, m_instanceBuffer.get(), staged.offset, 0, bytes);
		m_drawStats.uploadedSlots = slotCount;
	}
	else
	{
		// One copy per run of consecutive slots.
		std::sort(pending.begin(), pending.end());
		for (size_t runStart = 0; runStart < pending.size();)
		{
			size_t runEnd = runStart + 1;
			while (runEnd < pending.size() && pending[runEnd] == pending[runEnd - 1] + 1)
			{
				++runEnd;
			}

			const u32 firstSlot = pending[runStart];
			const u64 bytes		= static_cast<u64>(runEnd - runStart) * sizeof(InstanceData);
			UploadResult staged = m_uploadBuffer->AllocAndCopy(&instances[firstSlot], bytes);
			cmd.CopyBuffer(staging, m_instanceBuffer.get(), staged.offset,
						   static_cast<u64>(firstSlot) * sizeof(InstanceData), bytes);

			runStart = runEnd;
		}
		m_drawStats.uploadedSlots = static_cast<u32>(pending.size());
	}

	cmd.TransitionBuffer(m_instanceBuffer.get(), ResourceState::ShaderResource);
	m_scene.ClearUploads();
}

void Renderer::UploadSlotIndices(CommandList& cmd, const Vector<u32>& indices, Buffer* buffer)
{
	if (indices.empty())
	{
		return;
	}

	const u64 bytes = static_cast<u64>(indices.size()) * sizeof(u32);
	DYNAMIC_ASSERT(buffer && buffer->GetSize() >= bytes,
				   "Renderer::UploadSlotIndices: grow it in GrowSceneBuffers first");

	UploadResult staged = m_uploadBuffer->AllocAndCopy(indices.data(), bytes);

	cmd.TransitionBuffer(buffer, ResourceState::CopyDest);
	cmd.CopyBuffer(m_uploadBuffer->GetBackingBuffer(), buffer, staged.offset, 0, bytes);
	cmd.TransitionBuffer(buffer, ResourceState::ShaderResource);
}

void Renderer::UploadDrawArgs(CommandList& cmd, const Vector<DrawIndexedArgs>& args, Buffer* buffer,
							  ResourceState finalState)
{
	if (args.empty())
	{
		return;
	}

	const u64 bytes = static_cast<u64>(args.size()) * sizeof(DrawIndexedArgs);
	DYNAMIC_ASSERT(buffer && buffer->GetSize() >= bytes, "Renderer::UploadDrawArgs: grow it in GrowSceneBuffers first");

	UploadResult staged = m_uploadBuffer->AllocAndCopy(args.data(), bytes);

	// Also resets last frame's GPU counts, since every record is rewritten.
	cmd.TransitionBuffer(buffer, ResourceState::CopyDest);
	cmd.CopyBuffer(m_uploadBuffer->GetBackingBuffer(), buffer, staged.offset, 0, bytes);
	cmd.TransitionBuffer(buffer, finalState);
}

bool Renderer::GrowSceneBuffers(const RenderScene::CullRegions& regions, bool bGPUCulling)
{
	const u32 slotCount	 = static_cast<u32>(m_scene.GetInstances().size());
	const u32 batchCount = m_scene.GetBatchCount();

	// A regrown instance buffer starts empty, so the caller has to upload every slot.
	bool bInstancesGrown = false;
	if (slotCount > 0)
	{
		bInstancesGrown = EnsureBufferCapacity(m_instanceBuffer, m_instanceCapacity, slotCount, sizeof(InstanceData),
											   "InstanceBuffer");
	}

	// UAVs in both cull modes, so switching never needs a new buffer.
	if (batchCount > 0)
	{
		EnsureBufferCapacity(m_drawArgsBuffer, m_drawArgsCapacity, batchCount, sizeof(DrawIndexedArgs),
							 "DrawArgsBuffer", true);
		EnsureBufferCapacity(m_shadowDrawArgsBuffer, m_shadowDrawArgsCapacity, batchCount, sizeof(DrawIndexedArgs),
							 "ShadowDrawArgsBuffer", true);
	}

	if (bGPUCulling)
	{
		if (regions.visibleListSize > 0)
		{
			EnsureBufferCapacity(m_regionStartBuffer, m_regionStartCapacity,
								 static_cast<u32>(regions.regionStarts.size()), sizeof(u32), "RegionStartBuffer");

			// Sized for the worst case, every member visible. The cull fills a prefix
			// of each batch's region and the rest is never read.
			EnsureBufferCapacity(m_slotIndexBuffer, m_slotIndexCapacity, regions.visibleListSize, sizeof(u32),
								 "SlotIndexBuffer", true);
			EnsureBufferCapacity(m_shadowSlotIndexBuffer, m_shadowSlotIndexCapacity, regions.visibleListSize,
								 sizeof(u32), "ShadowSlotIndexBuffer", true);
		}
	}
	else
	{
		if (!m_cameraCull.indices.empty())
		{
			EnsureBufferCapacity(m_slotIndexBuffer, m_slotIndexCapacity, static_cast<u32>(m_cameraCull.indices.size()),
								 sizeof(u32), "SlotIndexBuffer", true);
		}
		if (!m_shadowCull.indices.empty())
		{
			EnsureBufferCapacity(m_shadowSlotIndexBuffer, m_shadowSlotIndexCapacity,
								 static_cast<u32>(m_shadowCull.indices.size()), sizeof(u32), "ShadowSlotIndexBuffer",
								 true);
		}
	}

	return bInstancesGrown;
}

void Renderer::AddSceneUploadPass(bool bInstancesGrown, bool bGPUCulling, Vector<u32> regionStarts,
								  Vector<DrawIndexedArgs> cameraArgs, Vector<DrawIndexedArgs> shadowArgs)
{
	m_renderGraph.AddPass("Scene Upload")
		.Execute(
			[this, bInstancesGrown, bGPUCulling, regionStarts = std::move(regionStarts),
			 cameraArgs = std::move(cameraArgs), shadowArgs = std::move(shadowArgs)](CommandList& cmd)
			{
				// Both draw passes read the same instance buffer, so it is written once up front.
				UploadInstances(cmd, bInstancesGrown);

				// The GPU cull counts into the args, so they stay writable until it is done.
				const ResourceState argsState =
					bGPUCulling ? ResourceState::UnorderedAccess : ResourceState::IndirectArgument;
				UploadDrawArgs(cmd, cameraArgs, m_drawArgsBuffer.get(), argsState);
				UploadDrawArgs(cmd, shadowArgs, m_shadowDrawArgsBuffer.get(), argsState);

				if (bGPUCulling)
				{
					// The cull's input. It writes the visible indices itself.
					UploadSlotIndices(cmd, regionStarts, m_regionStartBuffer.get());
				}
				else
				{
					// The CPU cull already knows the visible indices.
					UploadSlotIndices(cmd, m_cameraCull.indices, m_slotIndexBuffer.get());
					UploadSlotIndices(cmd, m_shadowCull.indices, m_shadowSlotIndexBuffer.get());
				}
			});
}

void Renderer::AddArgsReadbackPass(bool bHasShadowArgs)
{
	// Only the GPU knows the final instance counts, so they come back for the stats.
	// After both draw passes, which read the args as indirect arguments.
	m_renderGraph.AddPass("Args Readback")
		.Execute(
			[this, bHasShadowArgs](CommandList& cmd)
			{
				const u32 batchCount = m_scene.GetBatchCount();
				RecordArgsReadback(cmd, DrawPass::Camera, m_drawArgsBuffer.get(), batchCount);
				RecordArgsReadback(cmd, DrawPass::Shadow, bHasShadowArgs ? m_shadowDrawArgsBuffer.get() : nullptr,
								   batchCount);
			});
}

Vector<DrawIndexedArgs> Renderer::BuildDrawArgs() const
{
	Vector<DrawIndexedArgs> args(m_scene.GetBatchCount());

	for (u32 batch = 0; batch < static_cast<u32>(args.size()); ++batch)
	{
		const RenderScene::Batch& info = m_scene.GetBatch(batch);

		const u32 meshHandle   = static_cast<u32>(info.key >> 32);
		const u32 submeshIndex = static_cast<u32>(info.key & 0xFFFFFFFF);

		const MeshResource* resource = m_resourceManager->GetMeshResourceByHandle(meshHandle);
		if (!resource)
		{
			continue;
		}

		const Submesh& submesh = resource->mesh->submeshes[submeshIndex];
		args[batch].indexCount = submesh.indexCount;
		args[batch].firstIndex = submesh.indexOffset;
		args[batch].baseVertex = static_cast<int32>(submesh.vertexOffset);
	}

	return args;
}

void Renderer::CreatePipelines()
{
	m_cullPipeline				= CreateCullPassPipeline(*m_pipelines);
	m_directionalShadowPipeline = CreateShadowPassPipeline(*m_pipelines);
	m_deferredGeomPipeline		= CreateGBufferPassPipeline(*m_pipelines);
	m_deferredLightPipeline		= CreateLightingPassPipeline(*m_pipelines);
	m_tonemapPipeline			= CreateTonemapPassPipeline(*m_pipelines);
}

void Renderer::RecordArgsReadback(CommandList& cmd, DrawPass pass, Buffer* drawArgs, u32 count)
{
	ArgsReadback& readback = m_argsReadback[static_cast<u32>(pass)][m_frameIndex];
	readback.count		   = 0;

	if (!drawArgs || count == 0)
	{
		return;
	}

	const u64 bytes = static_cast<u64>(count) * sizeof(DrawIndexedArgs);

	// This slot's fence passed in BeginFrame, so the old buffer is free to replace.
	if (!readback.buffer || readback.buffer->GetSize() < bytes)
	{
		BufferDesc desc;
		desc.type		 = BufferType::Readback;
		desc.numElements = count;
		desc.stride		 = sizeof(DrawIndexedArgs);
		desc.name		 = pass == DrawPass::Camera ? "CameraArgsReadback" : "ShadowArgsReadback";
		readback.buffer	 = m_device->CreateBuffer(desc);
	}

	cmd.TransitionBuffer(drawArgs, ResourceState::CopySource);
	cmd.CopyBuffer(drawArgs, readback.buffer.get(), 0, 0, bytes);
	readback.count = count;
}

Renderer::ReadbackStats Renderer::ReadArgsReadback()
{
	ReadbackStats stats;

	for (u32 pass = 0; pass < static_cast<u32>(DrawPass::Count); ++pass)
	{
		const ArgsReadback& readback = m_argsReadback[pass][m_frameIndex];
		if (readback.count == 0)
		{
			continue;
		}

		const DrawIndexedArgs* args = static_cast<const DrawIndexedArgs*>(readback.buffer->Map());
		for (u32 batch = 0; batch < readback.count; ++batch)
		{
			// A zeroed record is a batch whose mesh resource was missing this frame.
			if (args[batch].indexCount == 0)
			{
				continue;
			}

			stats.numTris += (args[batch].indexCount / 3) * args[batch].instanceCount;
			if (pass == static_cast<u32>(DrawPass::Camera))
			{
				stats.visibleInstances += args[batch].instanceCount;
			}
		}
		readback.buffer->Unmap();
	}

	return stats;
}

Vector<BatchItem> Renderer::BuildBatchItems(const Vector<RenderScene::VisibleBatch>& batches) const
{
	Texture* defaultTexture			= m_resourceManager->GetDefaultTexture();
	Texture* defaultMaterialTexture = m_resourceManager->GetDefaultMaterialTexture();
	Texture* defaultNormalTexture	= m_resourceManager->GetDefaultNormalTexture();

	Vector<BatchItem> batchItems;
	batchItems.reserve(batches.size());

	for (const RenderScene::VisibleBatch& visible : batches)
	{
		const u64 key = m_scene.GetBatch(visible.batchIndex).key;

		u32 decodedMeshHandle	= static_cast<u32>(key >> 32);
		u32 decodedSubmeshIndex = static_cast<u32>(key & 0xFFFFFFFF);

		MeshResource* resource = m_resourceManager->GetMeshResourceByHandle(decodedMeshHandle);
		FATAL_ASSERT(resource, "Renderer::BuildBatchItems: MeshResource is invalid for a live batch");

		const Submesh& submesh		  = resource->mesh->submeshes[decodedSubmeshIndex];
		const Material& material	  = resource->mesh->materials[submesh.materialIndex];
		const Vector<u32>& texHandles = resource->textureHandles;

		BatchItem item;
		item.positionBuffer	 = resource->positionBuffer.get();
		item.attributeBuffer = resource->attributeBuffer.get();
		item.indexBuffer	 = resource->indexBuffer.get();
		item.indexCount		 = submesh.indexCount;
		item.indexOffset	 = submesh.indexOffset;
		item.vertexOffset	 = submesh.vertexOffset;
		item.emissiveFactor	 = material.emissiveFactor;
		item.batchIndex		 = visible.batchIndex;
		item.instanceCount	 = visible.count;
		item.instanceOffset	 = visible.offset;

		for (int slot = 0; slot < TextureSlot::TextureSlotCount; ++slot)
		{
			if (slot == TextureSlot::BaseColor)
			{
				item.textures[slot] = defaultTexture;
			}
			else if (slot == TextureSlot::Normal)
			{
				item.textures[slot] = defaultNormalTexture;
			}
			else
			{
				item.textures[slot] = defaultMaterialTexture;
			}

			int32 texIdx = material.TextureIndices[slot];
			if (texIdx >= 0 && texIdx < static_cast<int32>(texHandles.size()))
			{
				TextureResource* tex = m_resourceManager->GetTextureResourceByHandle(texHandles[texIdx]);
				if (tex)
				{
					item.textures[slot] = tex->gpuTexture.get();
				}
			}
		}

		batchItems.push_back(item);
	}

	return batchItems;
}

void Renderer::InitGBufferTextures()
{
	const u32 w = m_swapChain->GetWidth();
	const u32 h = m_swapChain->GetHeight();

	TextureDesc albedoDesc;
	albedoDesc.width  = w;
	albedoDesc.height = h;
	albedoDesc.format = TextureFormat::RGBA8;
	albedoDesc.usage  = TextureUsage::RenderTarget;

	TextureDesc normalDesc;
	normalDesc.width  = w;
	normalDesc.height = h;
	normalDesc.format = TextureFormat::RGBA16F;
	normalDesc.usage  = TextureUsage::RenderTarget;

	TextureDesc materialDesc;
	materialDesc.width	= w;
	materialDesc.height = h;
	materialDesc.format = TextureFormat::RGBA8;
	materialDesc.usage	= TextureUsage::RenderTarget;

	TextureDesc emissiveDesc;
	emissiveDesc.width	= w;
	emissiveDesc.height = h;
	emissiveDesc.format = TextureFormat::RGBA8;
	emissiveDesc.usage	= TextureUsage::RenderTarget;

	m_gbufferSimple.albedo	 = m_device->CreateTexture(albedoDesc);
	m_gbufferSimple.normal	 = m_device->CreateTexture(normalDesc);
	m_gbufferSimple.material = m_device->CreateTexture(materialDesc);
	m_gbufferSimple.emissive = m_device->CreateTexture(emissiveDesc);

	// Lighting writes here and tonemap reads it. Swap chain sized like the GBuffer,
	// so it is created and reset alongside it.
	TextureDesc hdrDesc;
	hdrDesc.width  = w;
	hdrDesc.height = h;
	hdrDesc.format = TextureFormat::RGBA16F;
	hdrDesc.usage  = TextureUsage::RenderTarget;
	m_hdrTarget	   = m_device->CreateTexture(hdrDesc);

	LOG_DEBUG("Renderer: G-buffer textures ready");
}

void Renderer::InitShadowTextures()
{
	TextureDesc directionalShadowDesc;
	directionalShadowDesc.width	 = m_shadowResolution;
	directionalShadowDesc.height = m_shadowResolution;
	directionalShadowDesc.format = TextureFormat::Depth32F;
	directionalShadowDesc.usage	 = TextureUsage::DepthStencilSampled;

	TextureDesc spotShadowMap;
	spotShadowMap.width	 = m_shadowResolution;
	spotShadowMap.height = m_shadowResolution;
	spotShadowMap.format = TextureFormat::Depth32F;
	spotShadowMap.usage	 = TextureUsage::DepthStencilSampled;

	m_shadowTextures.directionalShadowMap = m_device->CreateTexture(directionalShadowDesc);
	m_shadowTextures.spotShadowMap		  = m_device->CreateTexture(spotShadowMap);
}
