#include <Rendering/Renderer/Renderer.h>
#include <Rendering/Resource/ResourceManager.h>
#include <Rendering/Resource/MeshResource.h>
#include <Rendering/Resource/TextureResource.h>
#include <Rendering/Renderer/ResourceState.h>
#include <Core/ECS/World.h>
#include <Core/ECS/Components/TransformComponent.h>
#include <Core/ECS/Components/MeshComponent.h>
#include <Rendering/Mesh/Mesh.h>
#include <Debugging/Assert.h>
#include <Debugging/Logging.h>
#include <algorithm>
#include <cstring>

// Per-draw data uploaded to the GPU each draw call.
// Size must not exceed the Vulkan minimum push-constant guarantee (128 bytes).
struct PerDrawConstants
{
	Mat4 viewProj;
	Mat4 model;
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

	// Transition the current back buffer from present state to renderable.
	m_swapChain->TransitionToRenderTarget(cmd);

	// Bind and clear the back buffer + depth.
	DescriptorHandle rtv = m_swapChain->GetCurrentRTV();
	cmd.SetRenderTargets(1, &rtv, m_depthHandle);
	cmd.ClearRenderTarget(rtv, 0.1f, 0.1f, 0.1f, 1.f);
	if (m_depthHandle.IsValid())
		cmd.ClearDepthStencil(m_depthHandle, 1.f, 0);

	// Full-screen viewport and scissor.
	const f32 w = static_cast<f32>(m_swapChain->GetWidth());
	const f32 h = static_cast<f32>(m_swapChain->GetHeight());
	cmd.SetViewport(0.f, 0.f, w, h);
	cmd.SetScissorRect(0, 0, m_swapChain->GetWidth(), m_swapChain->GetHeight());

	// // Test triangle — drawn until a scene system replaces it.
	// if (m_testTriPSO)
	// {
	// 	cmd.SetPipelineState(m_testTriPSO.get());
	// 	cmd.SetPrimitiveTopology(PrimitiveTopology::TriangleList);
	// 	cmd.Draw(3);
	// }

	// Draw mesh entities from ECS.
	// Each<T...> iterates all entities that have AT LEAST the queried components.
	// Entities with additional components beyond Transform + Mesh are still included.
	if (!m_meshPSO)
	{
		CreateMeshPipeline();
	}

	if (m_world && m_resourceManager && m_meshPSO)
	{
		cmd.SetPipelineState(m_meshPSO.get());
		cmd.SetPrimitiveTopology(PrimitiveTopology::TriangleList);

		m_world->Each<TransformComponent, MeshComponent>(
			[&cmd, this](Entity entity, TransformComponent& transform, MeshComponent& meshComp)
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
				constants.viewProj = m_viewProj;
				{
					using namespace DirectX;
					SimdMat S = XMMatrixScaling(transform.scale.x, transform.scale.y, transform.scale.z);
					SimdMat R = XMMatrixRotationQuaternion(XMLoadFloat4(&transform.rotation));
					SimdMat T = XMMatrixTranslation(transform.position.x, transform.position.y, transform.position.z);
					XMStoreFloat4x4(&constants.model, XMMatrixMultiply(XMMatrixMultiply(S, R), T));
				}

				// Sub-allocate from ring buffer and bind as CBV (root CBV on D3D12, push descriptor UBO on Vulkan).
				UploadAllocation alloc = m_uploadBuffer->Alloc(sizeof(PerDrawConstants), 256);
				memcpy(alloc.cpuPtr, &constants, sizeof(PerDrawConstants));
				cmd.SetConstantBufferView(0, m_uploadBuffer->GetBackingBuffer(), alloc.offset, sizeof(PerDrawConstants));

				cmd.SetVertexBuffer(resource->vertexBuffer.get());
				cmd.SetIndexBuffer(resource->indexBuffer.get());

				Vector<Texture*> matTextures;

				for (const Submesh& submesh : resource->mesh->submeshes)
				{
					if (submesh.materialIndex >= 0)
					{
						const Material& mat = resource->mesh->materials[submesh.materialIndex];
						m_resourceManager->GetMaterialTextures(meshComp.path, *resource->mesh, mat, matTextures);
						cmd.SetShaderResources(1, matTextures);
					}

					cmd.DrawIndexed(submesh.indexCount, 1, submesh.indexOffset, submesh.vertexOffset, 0);
				}
			});
	}

	// TODO: G-buffer pass, lighting pass, post-process

	// Transition back buffer to present state before EndFrame submits the list.
	m_swapChain->TransitionToPresent(cmd);
}

void Renderer::DrawForwardPlus()
{
	// TODO: light culling compute pass, forward opaque pass
	// Shares the same back-buffer setup as deferred for now.
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
	meshDesc.textureSlotCount	  = 5; // BaseColor, Normal, MetallicRoughness, Occlusion, Emissive
	m_meshPSO					  = m_device->CreatePipelineState(meshDesc);

	LOG_DEBUG("Renderer: mesh PSO ready");
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
