#include "Renderer/DescriptorHandle.h"
#include "Renderer/Pipeline.h"
#include "Renderer/Texture.h"
#include <Rendering/Renderer/Renderer.h>
#include <Rendering/Resource/ResourceManager.h>
#include <Rendering/Resource/MeshResource.h>
#include <Rendering/Resource/TextureResource.h>
#include <Rendering/Renderer/ResourceState.h>
#include <Core/ECS/World.h>
#include <Core/ECS/Components/TransformComponent.h>
#include <Core/ECS/Components/MeshComponent.h>
#include <Core/ECS/Components/CameraComponent.h>
#include <Rendering/Mesh/Mesh.h>
#include <Debugging/Assert.h>
#include <Debugging/Logging.h>
#include <algorithm>

struct PerDrawConstants
{
	Mat4 viewProj;
	Mat4 model;
	Mat4 modelInvTranspose;
};

Renderer::~Renderer() = default;

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
	CommandList& cmd = *m_graphicsLists[0];

	// Find the active camera from the ECS.
	Mat4 viewProj		= {};
	Vec3 cameraPosition = {};
	bool hasCamera		= false;

	if (m_world)
	{
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
	}

	if (!m_deferredGeomPSO)
	{
		CreateDeferredGeometryPipeline();
	}

	if (!m_gbufferSimple.IsInitialized())
	{
		InitGBufferTextures();
	}

	// Bind and clear the back buffer + depth.
	Array<DescriptorHandle, 3> GBuffer{ m_gbufferSimple.albedo->GetRTV(), m_gbufferSimple.normal->GetRTV(),
										m_gbufferSimple.material->GetRTV() };

	cmd.TransitionTexture(m_gbufferSimple.albedo.get(), ResourceState::RenderTarget);
	cmd.TransitionTexture(m_gbufferSimple.normal.get(), ResourceState::RenderTarget);
	cmd.TransitionTexture(m_gbufferSimple.material.get(), ResourceState::RenderTarget);

	cmd.SetRenderTargets(3, GBuffer.data(), m_depthHandle);

	cmd.ClearRenderTarget(GBuffer[0], 0.0f, 0.0f, 0.0f, 0.0f);
	cmd.ClearRenderTarget(GBuffer[1], 0.0f, 0.0f, 0.0f, 0.0f);
	cmd.ClearRenderTarget(GBuffer[2], 0.0f, 0.0f, 0.0f, 0.0f);

	if (m_depthHandle.IsValid())
	{
		cmd.ClearDepthStencil(m_depthHandle, 1.f, 0);
	}

	// Full-screen viewport and scissor.
	const f32 swapChainWidth  = static_cast<f32>(m_swapChain->GetWidth());
	const f32 swapChainHeight = static_cast<f32>(m_swapChain->GetHeight());
	cmd.SetViewport(0.f, 0.f, swapChainWidth, swapChainHeight);
	cmd.SetScissorRect(0, 0, m_swapChain->GetWidth(), m_swapChain->GetHeight());

	// Deferred geometry pass
	if (m_world && m_resourceManager && hasCamera)
	{
		cmd.SetPipelineState(m_deferredGeomPSO.get());
		cmd.SetPrimitiveTopology(PrimitiveTopology::TriangleList);

		m_world->Each<TransformComponent, MeshComponent>(
			[&cmd, &viewProj, this](Entity entity, TransformComponent& transform, MeshComponent& meshComp)
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

				PerDrawConstants constants;
				constants.viewProj = viewProj;
				{
					using namespace DirectX;
					SimdMat S = XMMatrixScaling(transform.scale.x, transform.scale.y, transform.scale.z);
					SimdMat R = XMMatrixRotationQuaternion(XMLoadFloat4(&transform.rotation));
					SimdMat T = XMMatrixTranslation(transform.position.x, transform.position.y, transform.position.z);
					SimdMat M = XMMatrixMultiply(XMMatrixMultiply(S, R), T);
					XMStoreFloat4x4(&constants.model, M);
					XMStoreFloat4x4(&constants.modelInvTranspose, XMMatrixTranspose(XMMatrixInverse(nullptr, M)));
				}

				UploadResult upload = m_uploadBuffer->AllocAndCopy(&constants, sizeof(PerDrawConstants), 256);
				cmd.SetConstantBufferView(0, m_uploadBuffer->GetBackingBuffer(), upload.offset, upload.size);

				cmd.SetVertexBuffer(resource->vertexBuffer.get());
				cmd.SetIndexBuffer(resource->indexBuffer.get());

				Vector<Texture*> materialTextures(TextureSlot::TextureSlotCount, nullptr);

				for (const Submesh& submesh : resource->mesh->submeshes)
				{
					if (submesh.materialIndex < 0)
					{
						continue;
					}

					const Material& material   = resource->mesh->materials[submesh.materialIndex];
					const Vector<u32>& handles = resource->textureHandles;

					auto tryFillSlot = [&](int32 texIdx, u32 slot)
					{
						materialTextures[slot] = nullptr;
						if (texIdx >= 0 && texIdx < static_cast<int32>(handles.size()))
						{
							TextureResource* tex = m_resourceManager->GetTextureResourceByHandle(handles[texIdx]);
							if (tex)
							{
								materialTextures[slot] = tex->gpuTexture.get();
							}
						}
					};

					for (int slot = 0; slot < TextureSlot::TextureSlotCount; ++slot)
					{
						tryFillSlot(material.TextureIndices[slot], slot);
					}

					cmd.SetShaderResources(1, materialTextures);
					cmd.DrawIndexed(submesh.indexCount, 1, submesh.indexOffset, submesh.vertexOffset, 0);
				}
			});
	}

	// DrawMeshesUnlit(cmd);

	// GBuffer Lighting Pass
	if (!m_deferredLightPSO)
	{
		CreateDeferredLightingPipeline();
	}

	cmd.TransitionTexture(m_gbufferSimple.albedo.get(), ResourceState::ShaderResource);
	cmd.TransitionTexture(m_gbufferSimple.normal.get(), ResourceState::ShaderResource);
	cmd.TransitionTexture(m_gbufferSimple.material.get(), ResourceState::ShaderResource);

	m_swapChain->TransitionToRenderTarget(cmd);

	// TODO Gather lights, upload structured buffer, kick off triangle draw

	//  Transition back buffer to present state before EndFrame submits the list.
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
	meshDesc.renderTargetFormats  = { TextureFormat::RGBA8, TextureFormat::RGBA16F, TextureFormat::RGBA8 };
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
		{ BindingType::TextureTable, 0, TextureSlot::TextureSlotCount }, // rootIndex 1: t0-t4 — GBuffer textures
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

	m_gbufferSimple.albedo	 = m_device->CreateTexture(albedoDesc);
	m_gbufferSimple.normal	 = m_device->CreateTexture(normalDesc);
	m_gbufferSimple.material = m_device->CreateTexture(materialDesc);

	LOG_DEBUG("Renderer: G-buffer textures ready");
}

void Renderer::DrawMeshesUnlit(CommandList& cmd)
{
	Mat4 viewProj		= {};
	Vec3 cameraPosition = {};
	bool hasCamera		= false;

	if (m_world)
	{
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
	}

	if (!m_meshPSO)
	{
		CreateMeshPipeline();
	}

	if (m_world && m_resourceManager && m_meshPSO && hasCamera)
	{
		cmd.SetPipelineState(m_meshPSO.get());
		cmd.SetPrimitiveTopology(PrimitiveTopology::TriangleList);

		m_world->Each<TransformComponent, MeshComponent>(
			[&cmd, &viewProj, this](Entity entity, TransformComponent& transform, MeshComponent& meshComp)
			{
				if (!(meshComp.renderFlags & RenderFlags_Visible))
				{
					return;
				}

				if (meshComp.path[0] == '\0')
				{
					return; // No path set
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

				if (!resource || resource->state != AssetState::Ready)
				{
					return; // Still loading or uploading — skip this frame
				}

				// Build model matrix from TransformComponent (scale → rotate → translate).
				PerDrawConstants constants;
				constants.viewProj = viewProj;
				{
					using namespace DirectX;
					SimdMat S = XMMatrixScaling(transform.scale.x, transform.scale.y, transform.scale.z);
					SimdMat R = XMMatrixRotationQuaternion(XMLoadFloat4(&transform.rotation));
					SimdMat T = XMMatrixTranslation(transform.position.x, transform.position.y, transform.position.z);
					SimdMat M = XMMatrixMultiply(XMMatrixMultiply(S, R), T);
					XMStoreFloat4x4(&constants.model, M);
					XMStoreFloat4x4(&constants.modelInvTranspose, XMMatrixTranspose(XMMatrixInverse(nullptr, M)));
				}

				UploadResult upload = m_uploadBuffer->AllocAndCopy(&constants, sizeof(PerDrawConstants), 256);
				cmd.SetConstantBufferView(0, m_uploadBuffer->GetBackingBuffer(), upload.offset, upload.size);

				cmd.SetVertexBuffer(resource->vertexBuffer.get());
				cmd.SetIndexBuffer(resource->indexBuffer.get());

				Vector<Texture*> matTextures(TextureSlot::TextureSlotCount, nullptr);

				for (const Submesh& submesh : resource->mesh->submeshes)
				{
					if (submesh.materialIndex >= 0)
					{
						const Material& mat		   = resource->mesh->materials[submesh.materialIndex];
						const Vector<u32>& handles = resource->textureHandles;

						auto tryFillSlot = [&](int32 texIdx, u32 slot)
						{
							matTextures[slot] = nullptr;
							if (texIdx >= 0 && texIdx < static_cast<int32>(handles.size()))
							{
								TextureResource* tex = m_resourceManager->GetTextureResourceByHandle(handles[texIdx]);
								if (tex)
								{
									matTextures[slot] = tex->gpuTexture.get();
								}
							}
						};

						for (int32 slot = 0; slot < TextureSlot::TextureSlotCount; ++slot)
						{
							tryFillSlot(mat.TextureIndices[slot], slot);
						}

						cmd.SetShaderResources(1, matTextures);
					}

					cmd.DrawIndexed(submesh.indexCount, 1, submesh.indexOffset, submesh.vertexOffset, 0);
				}
			});
	}
}