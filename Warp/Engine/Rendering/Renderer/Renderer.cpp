#include "DirectXMath.h"
#include "Math/Math.h"
#include "Renderer/UploadBuffer.h"
#include <Rendering/Lighting/LightData.h>
#include <Rendering/Renderer/DescriptorHandle.h>
#include <Rendering/Renderer/Pipeline.h>
#include <Rendering/Renderer/Texture.h>
#include <Rendering/Renderer/Renderer.h>
#include <Rendering/Resource/ResourceManager.h>
#include <Rendering/Resource/MeshResource.h>
#include <Rendering/Resource/TextureResource.h>
#include <Rendering/Renderer/ResourceState.h>
#include <Core/ECS/World.h>
#include <Core/ECS/Components/TransformComponent.h>
#include <Core/ECS/Components/MeshComponent.h>
#include <Core/ECS/Components/CameraComponent.h>
#include <Core/ECS/Components/LightComponent.h>
#include <Core/ECS/Components/SkyLightComponent.h>
#include <Rendering/Mesh/Mesh.h>
#include <Rendering/Renderer/DrawList.h>
#include <Rendering/Window/Window.h>
#include <Debugging/Assert.h>
#include <Debugging/Logging.h>
#include <UI/ImGuiBackend.h>
#include <algorithm>

struct PerDrawConstants
{
	Mat4 viewProj;
	Mat4 model;
	Mat4 modelInvTranspose;
	Vec3 emissiveFactor;
	f32 padding;
};

struct ShadowCB
{
	Mat4 lightViewProj;
	Mat4 model;
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

void Renderer::Shutdown()
{
	if (m_device)
	{
		m_device->WaitForIdle();
	}

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

	if (m_device)
	{
		m_device->WaitForIdle();
	}

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
		m_resourceManager->ProcessPendingUploads();

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
			InFlightStaging entry;
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

		m_world->Each<TransformComponent, CameraComponent>(
			[&](Entity entity, TransformComponent& transform, CameraComponent& camera)
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
			return;
		}
	}

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
	// Gather draw list and light list from the ECS (single pass each).
	// These lists decouple the ECS from the renderer for the rest of the frame.
	// ---------------------------------------------------------------------------

	Texture* defaultTexture			= m_resourceManager->GetDefaultTexture();
	Texture* defaultMaterialTexture = m_resourceManager->GetDefaultMaterialTexture();
	Texture* defaultNormalTexture	= m_resourceManager->GetDefaultNormalTexture();

	DrawList drawList;
	m_world->Each<TransformComponent, MeshComponent>(
		[&](Entity entity, TransformComponent& transform, MeshComponent& meshComp)
		{
			if (!meshComp.HasRenderFlag(RenderFlags_Visible))
			{
				return;
			}

			if (!meshComp.IsValid())
			{
				return;
			}

			MeshResource* resource = nullptr;
			if (meshComp.IsHandleValid())
			{
				resource = m_resourceManager->GetMeshResourceByHandle(meshComp.meshHandle);
			}
			else
			{
				resource = m_resourceManager->GetMeshResource(meshComp.path);
				if (resource)
				{
					meshComp.meshHandle = resource->handle;
				}
			}

			if (!resource)
			{
				return;
			}

			Mat4 model;
			Mat4 modelInvTranspose;
			{
				using namespace DirectX;
				SimdMat S = XMMatrixScaling(transform.scale.x, transform.scale.y, transform.scale.z);
				SimdMat R = XMMatrixRotationQuaternion(XMLoadFloat4(&transform.rotation));
				SimdMat T = XMMatrixTranslation(transform.position.x, transform.position.y, transform.position.z);
				SimdMat M = XMMatrixMultiply(XMMatrixMultiply(S, R), T);
				XMStoreFloat4x4(&model, M);
				XMStoreFloat4x4(&modelInvTranspose, XMMatrixTranspose(XMMatrixInverse(nullptr, M)));
			}

			for (const Submesh& submesh : resource->mesh->submeshes)
			{
				if (submesh.materialIndex < 0)
				{
					continue;
				}

				const Material& material   = resource->mesh->materials[submesh.materialIndex];
				const Vector<u32>& handles = resource->textureHandles;

				DrawItem item;
				item.model			   = model;
				item.modelInvTranspose = modelInvTranspose;
				item.vertexBuffer	   = resource->vertexBuffer.get();
				item.indexBuffer	   = resource->indexBuffer.get();
				item.indexCount		   = submesh.indexCount;
				item.indexOffset	   = submesh.indexOffset;
				item.vertexOffset	   = submesh.vertexOffset;
				item.emissiveFactor	   = material.emissiveFactor;
				item.renderFlags	   = meshComp.renderFlags;

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
					if (texIdx >= 0 && texIdx < static_cast<int32>(handles.size()))
					{
						TextureResource* tex = m_resourceManager->GetTextureResourceByHandle(handles[texIdx]);
						if (tex)
						{
							item.textures[slot] = tex->gpuTexture.get();
						}
					}
				}

				u32 itemIndex = static_cast<u32>(drawList.items.size());
				drawList.items.push_back(item);

				if (meshComp.HasRenderFlag(RenderFlags_CastShadow))
				{
					drawList.shadowCasters.push_back(itemIndex);
				}

				if (meshComp.HasRenderFlag(RenderFlags_Unlit))
				{
					drawList.unlitMeshes.push_back(itemIndex);
				}
				else
				{
					drawList.litMeshes.push_back(itemIndex);
				}
			}
		});

	LightList lightList;
	m_world->Each<TransformComponent, LightComponent>(
		[&](Entity entity, TransformComponent& transform, LightComponent& lightComp)
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

	// Gather sky light — drives procedural sky and emits a directional light.
	SkyParameters skyParameters{};
	Vector<LightInfo> skyLightInfos;

	m_world->Each<TransformComponent, SkyLightComponent>(
		[&](Entity entity, TransformComponent& transform, SkyLightComponent& skyComp)
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

	// ---------------------------------------------------------------------------
	// Shadow pass — render depth from each shadow-casting directional light's POV
	// ---------------------------------------------------------------------------

	cmd.TransitionTexture(m_shadowTextures.directionalShadowMap.get(), ResourceState::DepthWrite);
	cmd.SetRenderTargets(0, nullptr, m_shadowTextures.directionalShadowMap.get());
	cmd.ClearDepthStencil(m_shadowTextures.directionalShadowMap.get(), 1.f, 0);

	cmd.SetViewport(0.f, 0.f, static_cast<f32>(m_shadowResolution), static_cast<f32>(m_shadowResolution));
	cmd.SetScissorRect(0, 0, m_shadowResolution, m_shadowResolution);

	cmd.SetPipelineState(m_directionalShadowPSO.get());
	cmd.SetPrimitiveTopology(PrimitiveTopology::TriangleList);

	// Build the light view-projection for each directional shadow caster.
	// For now this handles the sky light's directional light.
	// Store the last computed lightViewProj for the lighting pass.
	Mat4 directionalLightViewProj = {};

	for (const LightInfo& skyLight : skyLightInfos)
	{
		using namespace DirectX;

		XMVECTOR lightDir = XMVector3Normalize(XMLoadFloat3(&skyLight.direction));
		XMVECTOR lightPos = XMVectorScale(XMVectorNegate(lightDir), 50.f);
		XMVECTOR up       = XMVectorSet(0.f, 1.f, 0.f, 0.f);

		// Avoid degenerate up vector when light points nearly straight up or down.
		if (fabsf(XMVectorGetY(lightDir)) > 0.99f)
		{
			up = XMVectorSet(1.f, 0.f, 0.f, 0.f);
		}

		XMMATRIX lightView = XMMatrixLookToLH(lightPos, lightDir, up);

		float orthoHalfSize = 30.f;
		float nearPlane     = 0.1f;
		float farPlane      = 100.f;
		XMMATRIX lightProj  = XMMatrixOrthographicLH(orthoHalfSize * 2.f, orthoHalfSize * 2.f, nearPlane, farPlane);

		XMStoreFloat4x4(&directionalLightViewProj, XMMatrixMultiply(lightView, lightProj));

		for (u32 shadowCasterIndex : drawList.shadowCasters)
		{
			const DrawItem& item = drawList.items[shadowCasterIndex];

			ShadowCB shadowConstants;
			shadowConstants.lightViewProj = directionalLightViewProj;
			shadowConstants.model         = item.model;

			UploadResult upload = m_uploadBuffer->AllocAndCopy(&shadowConstants, sizeof(ShadowCB), 256);
			cmd.SetConstantBufferView(0, m_uploadBuffer->GetBackingBuffer(), upload.offset, upload.size);

			cmd.SetVertexBuffer(item.vertexBuffer);
			cmd.SetIndexBuffer(item.indexBuffer);
			cmd.DrawIndexed(item.indexCount, 1, item.indexOffset, item.vertexOffset, 0);
		}
	}

	// Transition shadow map for later use in the lighting pass.
	cmd.TransitionTexture(m_shadowTextures.directionalShadowMap.get(), ResourceState::ShaderResource);

	// ---------------------------------------------------------------------------
	// GBuffer geometry pass
	// ---------------------------------------------------------------------------

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

	for (const DrawItem& item : drawList.items)
	{
		PerDrawConstants constants;
		constants.viewProj			= viewProj;
		constants.model				= item.model;
		constants.modelInvTranspose = item.modelInvTranspose;
		constants.emissiveFactor	= item.emissiveFactor;

		UploadResult upload = m_uploadBuffer->AllocAndCopy(&constants, sizeof(PerDrawConstants), 256);
		cmd.SetConstantBufferView(0, m_uploadBuffer->GetBackingBuffer(), upload.offset, upload.size);

		cmd.SetVertexBuffer(item.vertexBuffer);
		cmd.SetIndexBuffer(item.indexBuffer);
		cmd.SetShaderResources(1, { item.textures[TextureSlot::BaseColor], item.textures[TextureSlot::Normal],
									item.textures[TextureSlot::MetallicRoughness],
									item.textures[TextureSlot::Occlusion], item.textures[TextureSlot::Emissive] });
		cmd.DrawIndexed(item.indexCount, 1, item.indexOffset, item.vertexOffset, 0);
	}

	// GBuffer Lighting Pass
	if (!m_deferredLightPSO)
	{
		CreateDeferredLightingPipeline();
	}

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
	lightConstants.shadowBias   = 0.005f;

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

	cmd.Draw(3);

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

	// Input layout must match the Vertex struct layout in Mesh.h exactly.
	PipelineDesc meshDesc;
	meshDesc.vertexShader = m_meshVS.get();
	meshDesc.pixelShader  = m_meshPS.get();
	meshDesc.inputLayout  = {
		 { "POSITION", 0, TextureFormat::RGB32F, 0, InputElement::AppendAligned },
		 { "NORMAL", 0, TextureFormat::RGB32F, 0, InputElement::AppendAligned },
		 { "TANGENT", 0, TextureFormat::RGBA32F, 0, InputElement::AppendAligned },
		 { "TEXCOORD", 0, TextureFormat::RG32F, 0, InputElement::AppendAligned },
		 { "TEXCOORD", 1, TextureFormat::RG32F, 0, InputElement::AppendAligned },
		 { "COLOR", 0, TextureFormat::RGBA32F, 0, InputElement::AppendAligned },
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
		{ BindingType::ConstantBuffer, 0, 1 }, // rootIndex 0: b0 — per-draw constants
		{ BindingType::TextureTable, 0, TextureSlot::TextureSlotCount }, // rootIndex 1: t0-t4 — material textures
	};
	meshDesc.samplers = {
		{ 0, SamplerFilter::Linear, SamplerAddressMode::Wrap },
	};
	m_meshPSO = m_device->CreatePipelineState(meshDesc);

	LOG_DEBUG("Renderer: mesh PSO ready");
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

	// Input layout must match the Vertex struct layout in Mesh.h exactly.
	PipelineDesc meshDesc;
	meshDesc.vertexShader = m_deferredGeomVS.get();
	meshDesc.pixelShader  = m_deferredGeomPS.get();
	meshDesc.inputLayout  = {
		 { "POSITION", 0, TextureFormat::RGB32F, 0, InputElement::AppendAligned },
		 { "NORMAL", 0, TextureFormat::RGB32F, 0, InputElement::AppendAligned },
		 { "TANGENT", 0, TextureFormat::RGBA32F, 0, InputElement::AppendAligned },
		 { "TEXCOORD", 0, TextureFormat::RG32F, 0, InputElement::AppendAligned },
		 { "TEXCOORD", 1, TextureFormat::RG32F, 0, InputElement::AppendAligned },
		 { "COLOR", 0, TextureFormat::RGBA32F, 0, InputElement::AppendAligned },
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
		{ BindingType::ConstantBuffer, 0, 1 }, // rootIndex 0: b0 — per-draw constants
		{ BindingType::TextureTable, 0, TextureSlot::TextureSlotCount }, // rootIndex 1: t0-t4 — material textures
	};
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
		{ BindingType::ConstantBuffer, 0, 1 },   // rootIndex 0: b0 — lighting constants
		{ BindingType::TextureTable, 0, 5 },     // rootIndex 1: t0-t4 — GBuffer textures (albedo, normal, material, depth, emissive)
		{ BindingType::StructuredBuffer, 5, 1 }, // rootIndex 2: t5 — Light Infos buffer
		{ BindingType::TextureTable, 6, 1 },     // rootIndex 3: t6 — Shadow map
	};
	desc.samplers = {
		{ 0, SamplerFilter::Point,            SamplerAddressMode::Clamp },  // s0 — GBuffer sampling
		{ 2, SamplerFilter::ComparisonLinear,  SamplerAddressMode::Border }, // s2 — Shadow map comparison
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
	desc.inputLayout  = {
		 { "POSITION", 0, TextureFormat::RGB32F, 0, InputElement::AppendAligned },
		 { "NORMAL", 0, TextureFormat::RGB32F, 0, InputElement::AppendAligned },
		 { "TANGENT", 0, TextureFormat::RGBA32F, 0, InputElement::AppendAligned },
		 { "TEXCOORD", 0, TextureFormat::RG32F, 0, InputElement::AppendAligned },
		 { "TEXCOORD", 1, TextureFormat::RG32F, 0, InputElement::AppendAligned },
		 { "COLOR", 0, TextureFormat::RGBA32F, 0, InputElement::AppendAligned },
	};
	desc.renderTargetFormats  = {};
	desc.depthFormat		  = TextureFormat::Depth32F;
	desc.topology			  = PrimitiveTopology::TriangleList;
	desc.enableDepthTest	  = true;
	desc.enableDepthWrite	  = true;
	desc.enableStencilTest	  = false;
	desc.enableBlending		  = false;
	desc.rasterState.cullMode = RasterizerState::CullMode::None;
	desc.rasterState.fillMode = RasterizerState::FillMode::Solid;
	desc.bindings			  = {
		{ BindingType::ConstantBuffer, 0, 1 }, // rootIndex 0: b0 — Shadow Constants
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