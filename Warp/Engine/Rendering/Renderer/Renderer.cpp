#include "DirectXMath.h"
#include "Math/Frustum.h"
#include "Math/Math.h"
#include <Core/ECS/Components/CameraComponent.h>
#include <Core/ECS/Components/LightComponent.h>
#include <Core/ECS/Components/MeshComponent.h>
#include <Core/ECS/Components/SkyLightComponent.h>
#include <Core/ECS/Components/TransformComponent.h>
#include <Core/ECS/World.h>
#include <Debugging/Assert.h>
#include <Debugging/GPUMarker.h>
#include <Debugging/Logging.h>
#include <Debugging/Profiler.h>
#include <Rendering/Lighting/LightData.h>
#include <Rendering/Mesh/Mesh.h>
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

// Split by update frequency. The per-view halves are bound once per pass; only
// the per-draw halves are re-uploaded for each submesh.
struct PerViewConstants
{
	Mat4 viewProj;
};

struct PerBatchConstants
{
	Vec3 emissiveFactor;
	uint instanceOffset;
};

struct ShadowViewConstants
{
	Mat4 lightViewProj;
};

struct ShadowDrawConstants
{
	uint instanceOffset;
	u32 padding[3];
};

// Matches CullConstants in InstanceCull.hlsl.
struct CullConstants
{
	Vec4 planes[6];
	u32 slotCount;
	u32 requiredFlags;
	u32 padding[2];
};

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

	// Command lists
	m_graphicsLists.push_back(m_device->CreateCommandList(CommandQueueType::Graphics, k_framesInFlight));
	m_computeLists.push_back(m_device->CreateCommandList(CommandQueueType::Compute, k_framesInFlight));
	m_copyList		 = m_device->CreateCommandList(CommandQueueType::Copy, k_framesInFlight);
	m_urgentCopyList = m_device->CreateCommandList(CommandQueueType::Copy, k_framesInFlight);

	// Upload buffer
	m_uploadBuffer = m_device->CreateUploadBuffer(k_uploadHeapSize, k_framesInFlight);

	// Worker thread pool
	m_workerPool = std::make_unique<ThreadPool>(8);

	LOG_DEBUG("Renderer initialized ({})", m_device->GetAPIName());
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

	ShutdownImGui();

	if (m_swapChain)
	{
		m_swapChain->Cleanup();
	}

	LOG_DEBUG("Renderer shut down");
}

// ---------------------------------------------------------------------------
// ImGui integration — delegates to the platform-specific ImGuiBackend.
// ---------------------------------------------------------------------------

void Renderer::InitImGui(IWindow* window)
{
	m_imguiBackend	   = CreateImGuiBackend();
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

void Renderer::RenderImGui()
{
	if (m_imguiBackend && !m_graphicsLists.empty())
	{
		m_imguiBackend->Render(m_graphicsLists[0].get());
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

	CommandList& cmd = *m_graphicsLists[0];

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

	if (!m_deferredGeomPSO)
	{
		CreateDeferredGeometryPipeline();
	}

	if (!m_gbufferSimple.IsInitialized())
	{
		InitGBufferTextures();
	}

	if (!m_directionalShadowPSO)
	{
		InitShadowPass();
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
	Vector<DrawIndexedArgs> cameraArgs	  = BuildDrawArgs();
	Vector<DrawIndexedArgs> shadowArgs	  = cameraArgs;

	if (m_bGPUCulling)
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

	// Both passes read the same instance buffer, so it is written once up front.
	UploadInstances(cmd);

	// The GPU cull counts into the args, so they stay writable until it is done.
	const ResourceState argsState = m_bGPUCulling ? ResourceState::UnorderedAccess : ResourceState::IndirectArgument;
	UploadDrawArgs(cmd, cameraArgs, m_drawArgsBuffer, m_drawArgsCapacity, "DrawArgsBuffer", argsState);
	UploadDrawArgs(cmd, shadowArgs, m_shadowDrawArgsBuffer, m_shadowDrawArgsCapacity, "ShadowDrawArgsBuffer",
				   argsState);

	if (m_bGPUCulling)
	{
		if (regions.visibleListSize > 0)
		{
			UploadSlotIndices(cmd, regions.regionStarts, m_regionStartBuffer, m_regionStartCapacity, "RegionStartBuffer");

			// Sized for the worst case, every member visible. The cull fills a prefix
			// of each batch's region and the rest is never read.
			EnsureBufferCapacity(m_slotIndexBuffer, m_slotIndexCapacity, regions.visibleListSize, sizeof(u32),
								 "SlotIndexBuffer", true);
			EnsureBufferCapacity(m_shadowSlotIndexBuffer, m_shadowSlotIndexCapacity, regions.visibleListSize, sizeof(u32),
								 "ShadowSlotIndexBuffer", true);

			DispatchCull(cmd, cameraFrustum, cameraFlags, m_drawArgsBuffer.get(), m_slotIndexBuffer.get());

			if (hasDirectionalShadow)
			{
				DispatchCull(cmd, shadowFrustum, shadowFlags, m_shadowDrawArgsBuffer.get(),
							 m_shadowSlotIndexBuffer.get());
			}
		}
	}
	else
	{
		UploadSlotIndices(cmd, m_cameraCull.indices, m_slotIndexBuffer, m_slotIndexCapacity, "SlotIndexBuffer", true);
		UploadSlotIndices(cmd, m_shadowCull.indices, m_shadowSlotIndexBuffer, m_shadowSlotIndexCapacity,
						  "ShadowSlotIndexBuffer", true);
	}

	// ---------------------------------------------------------------------------
	// Shadow pass — render depth from each shadow-casting directional light's POV
	// ---------------------------------------------------------------------------

	Warp::Debugging::GPUMarker::BeginEvent(&cmd, "Shadow Pass");

	cmd.TransitionTexture(m_shadowTextures.directionalShadowMap.get(), ResourceState::DepthWrite);
	cmd.SetRenderTargets(0, nullptr, m_shadowTextures.directionalShadowMap.get());
	cmd.ClearDepthStencil(m_shadowTextures.directionalShadowMap.get(), 1.f, 0);

	cmd.SetViewport(0.f, 0.f, static_cast<f32>(m_shadowResolution), static_cast<f32>(m_shadowResolution));
	cmd.SetScissorRect(0, 0, m_shadowResolution, m_shadowResolution);

	cmd.SetPipelineState(m_directionalShadowPSO.get());
	cmd.SetPrimitiveTopology(PrimitiveTopology::TriangleList);

	// One directional light drives the shadow map. Its matrix and frustum were built
	// before the gather so casters could be culled against it there.
	if (hasDirectionalShadow)
	{
		ShadowViewConstants shadowView;
		shadowView.lightViewProj = directionalLightViewProj;

		UploadResult viewUpload = m_uploadBuffer->AllocAndCopy(&shadowView, sizeof(ShadowViewConstants), 256);
		cmd.SetConstantBufferView(2, m_uploadBuffer->GetBackingBuffer(), viewUpload.offset, viewUpload.size);

		if (!m_drawList.shadowBatchItems.empty())
		{
			cmd.SetShaderResourceBuffer(0, m_instanceBuffer.get(), 0);
			cmd.SetShaderResourceBuffer(3, m_shadowSlotIndexBuffer.get(), 0);
		}

		for (u32 batchIndex = 0; batchIndex < static_cast<u32>(m_drawList.shadowBatchItems.size()); ++batchIndex)
		{
			const BatchItem& shadowCasterBatch = m_drawList.shadowBatchItems[batchIndex];

			ShadowDrawConstants shadowBatchConstants;
			shadowBatchConstants.instanceOffset = shadowCasterBatch.instanceOffset;

			UploadResult shadowBatchConstantsUpload =
				m_uploadBuffer->AllocAndCopy(&shadowBatchConstants, sizeof(ShadowDrawConstants), 256);
			cmd.SetConstantBufferView(1, m_uploadBuffer->GetBackingBuffer(), shadowBatchConstantsUpload.offset,
									  shadowBatchConstantsUpload.size);

			// Depth only: position is the sole stream this pass reads.
			cmd.SetVertexBuffer(shadowCasterBatch.positionBuffer);
			cmd.SetIndexBuffer(shadowCasterBatch.indexBuffer);

			++m_drawStats.drawCalls;

			cmd.DrawIndexedIndirect(m_shadowDrawArgsBuffer.get(), shadowCasterBatch.batchIndex * sizeof(DrawIndexedArgs));
		}
	}
	// Transition shadow map for later use in the lighting pass.
	cmd.TransitionTexture(m_shadowTextures.directionalShadowMap.get(), ResourceState::ShaderResource);

	Warp::Debugging::GPUMarker::EndEvent(&cmd);

	// ---------------------------------------------------------------------------
	// GBuffer geometry pass
	// ---------------------------------------------------------------------------

	Warp::Debugging::GPUMarker::BeginEvent(&cmd, "GBuffer Geometry");

	Array<DescriptorHandle, 4> GBuffer{ m_gbufferSimple.albedo->GetRTV(), m_gbufferSimple.normal->GetRTV(),
										m_gbufferSimple.material->GetRTV(), m_gbufferSimple.emissive->GetRTV() };

	cmd.TransitionTexture(m_gbufferSimple.albedo.get(), ResourceState::RenderTarget);
	cmd.TransitionTexture(m_gbufferSimple.normal.get(), ResourceState::RenderTarget);
	cmd.TransitionTexture(m_gbufferSimple.material.get(), ResourceState::RenderTarget);
	cmd.TransitionTexture(m_gbufferSimple.emissive.get(), ResourceState::RenderTarget);

	cmd.SetRenderTargets(4, GBuffer.data(), m_depthTexture.get());

	cmd.ClearRenderTarget(GBuffer[0], 0.0f, 0.0f, 0.0f, 0.0f);
	cmd.ClearRenderTarget(GBuffer[1], 0.0f, 0.0f, 0.0f, 0.0f);
	cmd.ClearRenderTarget(GBuffer[2], 0.0f, 0.0f, 0.0f, 0.0f);
	cmd.ClearRenderTarget(GBuffer[3], 0.0f, 0.0f, 0.0f, 0.0f);

	if (m_depthTexture)
	{
		cmd.ClearDepthStencil(m_depthTexture.get(), 1.f, 0);
	}

	const f32 swapChainWidth  = static_cast<f32>(m_swapChain->GetWidth());
	const f32 swapChainHeight = static_cast<f32>(m_swapChain->GetHeight());
	cmd.SetViewport(0.f, 0.f, swapChainWidth, swapChainHeight);
	cmd.SetScissorRect(0, 0, m_swapChain->GetWidth(), m_swapChain->GetHeight());

	cmd.SetPipelineState(m_deferredGeomPSO.get());
	cmd.SetPrimitiveTopology(PrimitiveTopology::TriangleList);

	// Bound once for the whole pass rather than re-uploaded per submesh.
	PerViewConstants perView;
	perView.viewProj = viewProj;

	UploadResult perViewUpload = m_uploadBuffer->AllocAndCopy(&perView, sizeof(PerViewConstants), 256);
	cmd.SetConstantBufferView(2, m_uploadBuffer->GetBackingBuffer(), perViewUpload.offset, perViewUpload.size);

	if (!m_drawList.batchItems.empty())
	{
		cmd.SetShaderResourceBuffer(3, m_instanceBuffer.get(), 0);
		cmd.SetShaderResourceBuffer(4, m_slotIndexBuffer.get(), 0);
	}

	for (u32 batchIndex = 0; batchIndex < static_cast<u32>(m_drawList.batchItems.size()); ++batchIndex)
	{
		const BatchItem& item = m_drawList.batchItems[batchIndex];

		PerBatchConstants batchConstants;
		batchConstants.emissiveFactor = item.emissiveFactor;
		batchConstants.instanceOffset = item.instanceOffset;

		UploadResult upload = m_uploadBuffer->AllocAndCopy(&batchConstants, sizeof(PerBatchConstants), 256);
		cmd.SetConstantBufferView(0, m_uploadBuffer->GetBackingBuffer(), upload.offset, upload.size);

		Buffer* streams[] = { item.positionBuffer, item.attributeBuffer };
		cmd.SetVertexBuffers(streams, 2);
		cmd.SetIndexBuffer(item.indexBuffer);
		cmd.SetShaderResources(1, { item.textures[TextureSlot::BaseColor], item.textures[TextureSlot::Normal],
									item.textures[TextureSlot::MetallicRoughness],
									item.textures[TextureSlot::Occlusion], item.textures[TextureSlot::Emissive] });

		++m_drawStats.drawCalls;

		cmd.DrawIndexedIndirect(m_drawArgsBuffer.get(), item.batchIndex * sizeof(DrawIndexedArgs));
	}

	Warp::Debugging::GPUMarker::EndEvent(&cmd);

	// Only the GPU knows the final instance counts, so they come back for the stats.
	const u32 batchCount = m_scene.GetBatchCount();
	RecordArgsReadback(cmd, DrawPass::Camera, m_drawArgsBuffer.get(), batchCount);
	RecordArgsReadback(cmd, DrawPass::Shadow, hasDirectionalShadow ? m_shadowDrawArgsBuffer.get() : nullptr,
					   batchCount);

	// GBuffer Lighting Pass
	if (!m_deferredLightPSO)
	{
		CreateDeferredLightingPipeline();
	}

	Warp::Debugging::GPUMarker::BeginEvent(&cmd, "Deferred Lighting");

	cmd.TransitionTexture(m_gbufferSimple.albedo.get(), ResourceState::ShaderResource);
	cmd.TransitionTexture(m_gbufferSimple.normal.get(), ResourceState::ShaderResource);
	cmd.TransitionTexture(m_gbufferSimple.material.get(), ResourceState::ShaderResource);
	cmd.TransitionTexture(m_gbufferSimple.emissive.get(), ResourceState::ShaderResource);
	cmd.TransitionTexture(m_depthTexture.get(), ResourceState::ShaderResource);

	cmd.SetPipelineState(m_deferredLightPSO.get());
	cmd.SetPrimitiveTopology(PrimitiveTopology::TriangleList);

	m_swapChain->TransitionToRenderTarget(cmd);
	DescriptorHandle backBufferRTV = m_swapChain->GetCurrentRTV();
	cmd.SetRenderTargets(1, &backBufferRTV, nullptr);

	LightPassConstants lightConstants;
	{
		using namespace DirectX;
		SimdMat simdViewProj	= XMLoadFloat4x4(&viewProj);
		SimdMat simdInvViewProj = XMMatrixInverse(nullptr, simdViewProj);
		XMStoreFloat4x4(&lightConstants.invViewProj, simdInvViewProj);
	}
	lightConstants.cameraPosition = cameraPosition;

	// Convert gathered LightList to GPU-ready LightInfo array.
	Vector<LightInfo> lightInfos;
	lightInfos.reserve(lightList.items.size() + 1); // +1 for potential sky directional light

	for (const LightItem& light : lightList.items)
	{
		LightInfo info;
		info.position		= light.position;
		info.range			= light.range;
		info.color			= light.color;
		info.intensity		= light.intensity;
		info.direction		= light.direction;
		info.type			= static_cast<int32>(light.type);
		info.innerConeAngle = light.innerConeAngle;
		info.outerConeAngle = light.outerConeAngle;
		lightInfos.push_back(info);
	}

	// Sky light — still gathered from ECS here since it's a unique component
	// that drives both the procedural sky and a directional light.
	lightConstants.sky = skyParameters;
	if (!skyLightInfos.empty())
	{
		for (const LightInfo& sunLight : skyLightInfos)
		{
			lightInfos.push_back(sunLight);
		}
	}

	lightConstants.lightViewProj = directionalLightViewProj;
	lightConstants.shadowBias	 = 0.005f;

	lightConstants.lightCount		  = static_cast<int32>(lightInfos.size());
	UploadResult lightConstantsUpload = m_uploadBuffer->AllocAndCopy(&lightConstants, sizeof(LightPassConstants), 256);
	cmd.SetConstantBufferView(0, m_uploadBuffer->GetBackingBuffer(), lightConstantsUpload.offset,
							  lightConstantsUpload.size);

	cmd.SetShaderResources(1, { m_gbufferSimple.albedo.get(), m_gbufferSimple.normal.get(),
								m_gbufferSimple.material.get(), m_depthTexture.get(), m_gbufferSimple.emissive.get() });

	if (!lightInfos.empty())
	{
		UploadResult lightUpload =
			m_uploadBuffer->AllocAndCopy(lightInfos.data(), lightInfos.size() * sizeof(LightInfo));
		cmd.SetShaderResourceBuffer(2, m_uploadBuffer->GetBackingBuffer(), lightUpload.offset);
	}

	cmd.SetShaderResources(3, { m_shadowTextures.directionalShadowMap.get() });

	++m_drawStats.drawCalls;
	cmd.Draw(3);

	Warp::Debugging::GPUMarker::EndEvent(&cmd);

	// Render ImGui on top of the scene, before presenting.
	RenderImGui();

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

void Renderer::CreateMeshPipeline()
{
	ShaderDesc vsDesc;
	vsDesc.type		  = ShaderType::Vertex;
	vsDesc.entryPoint = "VSMain";
	vsDesc.filePath	  = "Shaders/Mesh.hlsl";
	m_meshVS		  = m_device->CreateShader(vsDesc);

	ShaderDesc psDesc;
	psDesc.type		  = ShaderType::Pixel;
	psDesc.entryPoint = "PSMain";
	psDesc.filePath	  = "Shaders/Mesh.hlsl";
	m_meshPS		  = m_device->CreateShader(psDesc);

	// Slot 0 is the position stream, slot 1 the VertexAttributes struct in Mesh.h.
	// Byte offsets append-align within each slot independently.
	PipelineDesc meshDesc;
	meshDesc.vertexShader = m_meshVS.get();
	meshDesc.pixelShader  = m_meshPS.get();
	meshDesc.inputLayout  = {
		{ "POSITION", 0, TextureFormat::RGB32F, 0, InputElement::AppendAligned },
		{ "NORMAL", 0, TextureFormat::RGB32F, 1, InputElement::AppendAligned },
		{ "TANGENT", 0, TextureFormat::RGBA32F, 1, InputElement::AppendAligned },
		{ "TEXCOORD", 0, TextureFormat::RG32F, 1, InputElement::AppendAligned },
		{ "TEXCOORD", 1, TextureFormat::RG32F, 1, InputElement::AppendAligned },
		{ "COLOR", 0, TextureFormat::RGBA32F, 1, InputElement::AppendAligned },
	};
	meshDesc.renderTargetFormats  = { TextureFormat::BGRA8 };
	meshDesc.depthFormat		  = TextureFormat::Depth32F;
	meshDesc.topology			  = PrimitiveTopology::TriangleList;
	meshDesc.enableDepthTest	  = true;
	meshDesc.enableDepthWrite	  = true;
	meshDesc.enableStencilTest	  = false;
	meshDesc.enableBlending		  = false;
	meshDesc.rasterState.cullMode = RasterizerState::CullMode::Back;
	meshDesc.rasterState.fillMode = RasterizerState::FillMode::Solid;
	meshDesc.bindings			  = {
		{ BindingType::ConstantBuffer, 0, 1 },							 // rootIndex 0: b0 — per-draw constants
		{ BindingType::TextureTable, 0, TextureSlot::TextureSlotCount }, // rootIndex 1: t0-t4 — material textures
	};
	meshDesc.samplers = {
		{ 0, SamplerFilter::Linear, SamplerAddressMode::Wrap },
	};
	m_meshPSO = m_device->CreatePipelineState(meshDesc);

	LOG_DEBUG("Renderer: mesh PSO ready");
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
	desc.type		 = BufferType::Structured;
	desc.numElements = newCapacity;
	desc.stride		 = stride;
	desc.name		 = name;
	desc.bUnorderedAccess = bUnorderedAccess;

	buffer	 = m_device->CreateBuffer(desc);
	capacity = newCapacity;

	LOG_DEBUG("Renderer: {} grown to {} elements", name, newCapacity);
	return true;
}

void Renderer::UploadInstances(CommandList& cmd)
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

	const bool bGrown =
		EnsureBufferCapacity(m_instanceBuffer, m_instanceCapacity, slotCount, sizeof(InstanceData), "InstanceBuffer");

	// A regrown buffer starts empty. Past a quarter dirty, one big copy beats
	// many small ones.
	const bool bFullUpload = bGrown || pending.size() * 4 > slotCount;
	if (!bFullUpload && pending.empty())
	{
		return;
	}

	Buffer* staging = m_uploadBuffer->GetBackingBuffer();
	cmd.TransitionBuffer(m_instanceBuffer.get(), ResourceState::CopyDest);

	if (bFullUpload)
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

void Renderer::UploadSlotIndices(CommandList& cmd, const Vector<u32>& indices, URef<Buffer>& buffer, u32& capacity,
								 const char* name, bool bUnorderedAccess)
{
	if (indices.empty())
	{
		return;
	}

	const u32 count = static_cast<u32>(indices.size());
	const u64 bytes = static_cast<u64>(count) * sizeof(u32);

	EnsureBufferCapacity(buffer, capacity, count, sizeof(u32), name, bUnorderedAccess);
	UploadResult staged = m_uploadBuffer->AllocAndCopy(indices.data(), bytes);

	cmd.TransitionBuffer(buffer.get(), ResourceState::CopyDest);
	cmd.CopyBuffer(m_uploadBuffer->GetBackingBuffer(), buffer.get(), staged.offset, 0, bytes);
	cmd.TransitionBuffer(buffer.get(), ResourceState::ShaderResource);
}

void Renderer::UploadDrawArgs(CommandList& cmd, const Vector<DrawIndexedArgs>& args, URef<Buffer>& buffer,
							  u32& capacity, const char* name, ResourceState finalState)
{
	if (args.empty())
	{
		return;
	}

	const u32 count = static_cast<u32>(args.size());
	const u64 bytes = static_cast<u64>(count) * sizeof(DrawIndexedArgs);

	// A UAV either way, so switching cull modes never needs a new buffer.
	EnsureBufferCapacity(buffer, capacity, count, sizeof(DrawIndexedArgs), name, true);
	UploadResult staged = m_uploadBuffer->AllocAndCopy(args.data(), bytes);

	// Also resets last frame's GPU counts, since every record is rewritten.
	cmd.TransitionBuffer(buffer.get(), ResourceState::CopyDest);
	cmd.CopyBuffer(m_uploadBuffer->GetBackingBuffer(), buffer.get(), staged.offset, 0, bytes);
	cmd.TransitionBuffer(buffer.get(), finalState);
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

		const Submesh& submesh	= resource->mesh->submeshes[submeshIndex];
		args[batch].indexCount	= submesh.indexCount;
		args[batch].firstIndex	= submesh.indexOffset;
		args[batch].baseVertex	= static_cast<int32>(submesh.vertexOffset);
	}

	return args;
}

void Renderer::CreateCullPipeline()
{
	ShaderDesc csDesc;
	csDesc.type		  = ShaderType::Compute;
	csDesc.entryPoint = "CSMain";
	csDesc.filePath	  = "Shaders/InstanceCull.hlsl";
	m_cullCS		  = m_device->CreateShader(csDesc);

	ComputePipelineDesc psoDesc;
	psoDesc.computeShader = m_cullCS.get();
	psoDesc.bindings	  = {
		 { BindingType::ConstantBuffer, 0, 1 },		// rootIndex 0: b0 — frustum, slot count, flags
		 { BindingType::StructuredBuffer, 0, 1 },	// rootIndex 1: t0 — instances
		 { BindingType::StructuredBuffer, 1, 1 },	// rootIndex 2: t1 — region starts
		 { BindingType::RWStructuredBuffer, 0, 1 }, // rootIndex 3: u0 — draw args
		 { BindingType::RWStructuredBuffer, 1, 1 }, // rootIndex 4: u1 — visible indices
	 };
	m_cullPSO = m_device->CreateComputePipelineState(psoDesc);

	LOG_DEBUG("Renderer: cull PSO ready");
}

void Renderer::DispatchCull(CommandList& cmd, const Array<Vec4, 6>& frustum, u32 requiredFlags, Buffer* drawArgs,
							Buffer* visibleIndices)
{
	if (!m_cullPSO)
	{
		CreateCullPipeline();
	}

	const u32 slotCount = static_cast<u32>(m_scene.GetInstances().size());

	CullConstants constants = {};
	for (u32 plane = 0; plane < 6; ++plane)
	{
		constants.planes[plane] = frustum[plane];
	}
	constants.slotCount		= slotCount;
	constants.requiredFlags = requiredFlags;

	UploadResult upload = m_uploadBuffer->AllocAndCopy(&constants, sizeof(CullConstants), 256);

	// The instances and region starts were left in ShaderResource by their uploads.
	cmd.TransitionBuffer(drawArgs, ResourceState::UnorderedAccess);
	cmd.TransitionBuffer(visibleIndices, ResourceState::UnorderedAccess);

	cmd.SetComputePipelineState(m_cullPSO.get());
	cmd.SetConstantBufferView(0, m_uploadBuffer->GetBackingBuffer(), upload.offset, upload.size);
	cmd.SetShaderResourceBuffer(1, m_instanceBuffer.get(), 0);
	cmd.SetShaderResourceBuffer(2, m_regionStartBuffer.get(), 0);
	cmd.SetUnorderedAccessBuffer(3, drawArgs, 0);
	cmd.SetUnorderedAccessBuffer(4, visibleIndices, 0);
	cmd.Dispatch((slotCount + 63) / 64, 1, 1);

	// The draw reads the args as indirect arguments, not as a UAV. Missing this
	// transition is a device removal, not a visible bug.
	cmd.TransitionBuffer(drawArgs, ResourceState::IndirectArgument);
	cmd.TransitionBuffer(visibleIndices, ResourceState::ShaderResource);
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

void Renderer::CreateDeferredGeometryPipeline()
{
	ShaderDesc vsDesc;
	vsDesc.type		  = ShaderType::Vertex;
	vsDesc.entryPoint = "VSMain";
	vsDesc.filePath	  = "Shaders/DeferredGeometry.hlsl";
	m_deferredGeomVS  = m_device->CreateShader(vsDesc);

	ShaderDesc psDesc;
	psDesc.type		  = ShaderType::Pixel;
	psDesc.entryPoint = "PSMain";
	psDesc.filePath	  = "Shaders/DeferredGeometry.hlsl";
	m_deferredGeomPS  = m_device->CreateShader(psDesc);

	// Slot 0 is the position stream, slot 1 the VertexAttributes struct in Mesh.h.
	// Byte offsets append-align within each slot independently.
	PipelineDesc meshDesc;
	meshDesc.vertexShader = m_deferredGeomVS.get();
	meshDesc.pixelShader  = m_deferredGeomPS.get();
	meshDesc.inputLayout  = {
		{ "POSITION", 0, TextureFormat::RGB32F, 0, InputElement::AppendAligned },
		{ "NORMAL", 0, TextureFormat::RGB32F, 1, InputElement::AppendAligned },
		{ "TANGENT", 0, TextureFormat::RGBA32F, 1, InputElement::AppendAligned },
		{ "TEXCOORD", 0, TextureFormat::RG32F, 1, InputElement::AppendAligned },
		{ "TEXCOORD", 1, TextureFormat::RG32F, 1, InputElement::AppendAligned },
		{ "COLOR", 0, TextureFormat::RGBA32F, 1, InputElement::AppendAligned },
	};
	meshDesc.renderTargetFormats  = { TextureFormat::RGBA8, TextureFormat::RGBA16F, TextureFormat::RGBA8,
									  TextureFormat::RGBA8 };
	meshDesc.depthFormat		  = TextureFormat::Depth32F;
	meshDesc.topology			  = PrimitiveTopology::TriangleList;
	meshDesc.enableDepthTest	  = true;
	meshDesc.enableDepthWrite	  = true;
	meshDesc.enableStencilTest	  = false;
	meshDesc.enableBlending		  = false;
	meshDesc.rasterState.cullMode = RasterizerState::CullMode::Back;
	meshDesc.rasterState.fillMode = RasterizerState::FillMode::Solid;
	meshDesc.bindings			  = {
		{ BindingType::ConstantBuffer, 0, 1 },								 // rootIndex 0: b0 — per-draw constants
		{ BindingType::TextureTable, 0, TextureSlot::TextureSlotCount },	 // rootIndex 1: t0-t4 — material textures
		{ BindingType::ConstantBuffer, 1, 1 },								 // rootIndex 2: b1 — per-view constants
		{ BindingType::StructuredBuffer, TextureSlot::TextureSlotCount, 1 }, // rootIndex 3: t5 — instance data
		{ BindingType::StructuredBuffer, TextureSlot::TextureSlotCount + 1, 1 }
	}; // rootIndex 4: t6 — slot indices
	meshDesc.samplers = {
		{ 0, SamplerFilter::Linear, SamplerAddressMode::Wrap },
	};
	m_deferredGeomPSO = m_device->CreatePipelineState(meshDesc);

	LOG_DEBUG("Renderer: deferred geometry PSO ready");
}

void Renderer::CreateDeferredLightingPipeline()
{
	ShaderDesc vsDesc;
	vsDesc.type		  = ShaderType::Vertex;
	vsDesc.entryPoint = "VSMain";
	vsDesc.filePath	  = "Shaders/DeferredLighting.hlsl";
	m_deferredLightVS = m_device->CreateShader(vsDesc);

	ShaderDesc psDesc;
	psDesc.type		  = ShaderType::Pixel;
	psDesc.entryPoint = "PSMain";
	psDesc.filePath	  = "Shaders/DeferredLighting.hlsl";
	m_deferredLightPS = m_device->CreateShader(psDesc);

	PipelineDesc desc;
	desc.vertexShader		  = m_deferredLightVS.get();
	desc.pixelShader		  = m_deferredLightPS.get();
	desc.inputLayout		  = {};
	desc.renderTargetFormats  = { TextureFormat::BGRA8 };
	desc.depthFormat		  = TextureFormat::Unknown; // No depth
	desc.topology			  = PrimitiveTopology::TriangleList;
	desc.enableDepthTest	  = false;
	desc.enableDepthWrite	  = false;
	desc.enableStencilTest	  = false;
	desc.enableBlending		  = false;
	desc.rasterState.cullMode = RasterizerState::CullMode::None;
	desc.rasterState.fillMode = RasterizerState::FillMode::Solid;
	desc.bindings			  = {
		{ BindingType::ConstantBuffer, 0, 1 }, // rootIndex 0: b0 — lighting constants
		{ BindingType::TextureTable, 0,
		  5 }, // rootIndex 1: t0-t4 — GBuffer textures (albedo, normal, material, depth, emissive)
		{ BindingType::StructuredBuffer, 5, 1 }, // rootIndex 2: t5 — Light Infos buffer
		{ BindingType::TextureTable, 6, 1 },	 // rootIndex 3: t6 — Shadow map
	};
	desc.samplers = {
		{ 0, SamplerFilter::Point, SamplerAddressMode::Clamp },				// s0 — GBuffer sampling
		{ 2, SamplerFilter::ComparisonLinear, SamplerAddressMode::Border }, // s2 — Shadow map comparison
	};
	m_deferredLightPSO = m_device->CreatePipelineState(desc);

	LOG_DEBUG("Renderer: deferred lighting PSO ready");
}

void Renderer::CreateTestTriangle()
{
	ShaderDesc vsDesc;
	vsDesc.type		  = ShaderType::Vertex;
	vsDesc.entryPoint = "VSMain";
	vsDesc.filePath	  = "Shaders/TestTriangle.hlsl";
	m_testTriVS		  = m_device->CreateShader(vsDesc);

	ShaderDesc psDesc;
	psDesc.type		  = ShaderType::Pixel;
	psDesc.entryPoint = "PSMain";
	psDesc.filePath	  = "Shaders/TestTriangle.hlsl";
	m_testTriPS		  = m_device->CreateShader(psDesc);

	PipelineDesc triDesc;
	triDesc.vertexShader		 = m_testTriVS.get();
	triDesc.pixelShader			 = m_testTriPS.get();
	triDesc.inputLayout			 = {};
	triDesc.renderTargetFormats	 = { TextureFormat::BGRA8 };
	triDesc.depthFormat			 = TextureFormat::Depth32F;
	triDesc.topology			 = PrimitiveTopology::TriangleList;
	triDesc.enableDepthTest		 = false;
	triDesc.enableDepthWrite	 = false;
	triDesc.enableStencilTest	 = false;
	triDesc.enableBlending		 = false;
	triDesc.rasterState.cullMode = RasterizerState::CullMode::None;
	triDesc.rasterState.fillMode = RasterizerState::FillMode::Solid;
	m_testTriPSO				 = m_device->CreatePipelineState(triDesc);

	LOG_DEBUG("Renderer: test triangle PSO ready");
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

	LOG_DEBUG("Renderer: G-buffer textures ready");
}

void Renderer::InitShadowPass()
{
	if (!m_directionalShadowPSO)
	{
		InitShadowPSO();
	}

	if (!m_shadowTextures.IsInitialized())
	{
		InitShadowTextures();
	}
}

void Renderer::InitShadowPSO()
{
	ShaderDesc vsDesc;
	vsDesc.type			  = ShaderType::Vertex;
	vsDesc.entryPoint	  = "VSMain";
	vsDesc.filePath		  = "Shaders/DirectionalShadow.hlsl";
	m_directionalShadowVS = m_device->CreateShader(vsDesc);

	PipelineDesc desc;
	desc.vertexShader = m_directionalShadowVS.get();
	desc.pixelShader  = nullptr;
	// Position only. The shadow VS reads nothing else, and binding this stream
	// alone is what makes the split pay off.
	desc.inputLayout = {
		{ "POSITION", 0, TextureFormat::RGB32F, 0, InputElement::AppendAligned },
	};
	desc.renderTargetFormats = {};
	desc.depthFormat		 = TextureFormat::Depth32F;
	desc.topology			 = PrimitiveTopology::TriangleList;
	desc.enableDepthTest	 = true;
	desc.enableDepthWrite	 = true;
	desc.enableStencilTest	 = false;
	desc.enableBlending		 = false;

	// Same depth values as CullMode::None here, at roughly half the rasterization.
	desc.rasterState.cullMode = RasterizerState::CullMode::Back;
	desc.rasterState.fillMode = RasterizerState::FillMode::Solid;

	// Starting values. Tune against the scene.
	desc.rasterState.depthBias			  = 100;
	desc.rasterState.slopeScaledDepthBias = 2.f;
	desc.rasterState.depthBiasClamp		  = 0.f;

	desc.bindings = {
		{ BindingType::StructuredBuffer, 0, 1 }, // rootIndex 0: t0 — structured buffer with instance data
		{ BindingType::ConstantBuffer, 0, 1 },	 // rootIndex 1: b0 - shadow constants
		{ BindingType::ConstantBuffer, 1, 1 },	 // rootIndex 2: b1 — per-view lightViewProj
		{ BindingType::StructuredBuffer, 1, 1 }, // rootIndex 3: t1 — slot indices
	};

	m_directionalShadowPSO = m_device->CreatePipelineState(desc);

	LOG_DEBUG("Renderer: deferred lighting PSO ready");
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
