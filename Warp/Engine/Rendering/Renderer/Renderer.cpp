#include <Rendering/Renderer/Renderer.h>
#include <Rendering/Resource/ResourceManager.h>
#include <Rendering/Resource/MeshResource.h>
#include <Core/ECS/World.h>
#include <Core/ECS/Components/TransformComponent.h>
#include <Core/ECS/Components/MeshComponent.h>
#include <Rendering/Mesh/Mesh.h>
#include <Debugging/Assert.h>
#include <Debugging/Logging.h>
#include <algorithm>

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
		m_inFlightStagingBuffers.erase(
			std::remove_if(m_inFlightStagingBuffers.begin(),
			               m_inFlightStagingBuffers.end(),
			               [completedCopyValue](const InFlightStaging& s)
			               {
			                   return s.fenceValue <= completedCopyValue;
			               }),
			m_inFlightStagingBuffers.end());
	}

	// Retire the oldest frame's upload-buffer slice so the ring buffer can reuse it.
	m_uploadBuffer->OnBeginFrame();

	// Reset all CPU-side per-frame allocations.
	m_frameArena.Reset();

	// Reset cross-queue wait flags for the new frame.
	m_graphicsWaitOnCopy = false;
	m_computeWaitOnCopy  = false;

	// Open all command lists for this frame slot.
	for (URef<CommandList>& list : m_graphicsLists) { list->Begin(m_frameIndex); }
	for (URef<CommandList>& list : m_computeLists)  { list->Begin(m_frameIndex); }
	m_copyList->Begin(m_frameIndex);
	m_urgentCopyList->Begin(m_frameIndex);

	// Process ResourceManager: check async loads, create GPU buffers, drain staging uploads.
	if (m_resourceManager)
	{
		m_resourceManager->ProcessPendingUploads();
		Vector<PendingStagingUpload> uploads = m_resourceManager->DrainStagingUploads();
		for (PendingStagingUpload& upload : uploads)
		{
			QueueStagingUpload(upload);
		}
	}
}

void Renderer::Draw()
{
	switch (m_renderPath)
	{
		case RenderPath::Deferred:    DrawDeferred();    break;
		case RenderPath::ForwardPlus: DrawForwardPlus(); break;
	}
}

void Renderer::EndFrame()
{
	// Close all command lists.
	for (URef<CommandList>& list : m_graphicsLists) { list->End(); }
	for (URef<CommandList>& list : m_computeLists)  { list->End(); }

	u64 lastCopyFenceValue = 0;

	// --- Urgent copies: submitted first, cross-queue wait inserted so
	//     graphics/compute won't execute until these specific copies finish.
	if (!m_urgentUploads.empty())
	{
		for (PendingStagingUpload& upload : m_urgentUploads)
		{
			m_urgentCopyList->CopyBuffer(upload.stagingBuffer.get(), upload.destination,
			                             0, 0, upload.size);
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
			entry.fenceValue    = urgentFenceValue;
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
		m_copyList->CopyBuffer(upload.stagingBuffer.get(), upload.destination,
		                       0, 0, upload.size);
	}
	m_copyList->End();

	u64 deferredFenceValue = m_copyQueue->Submit(*m_copyList);

	for (PendingStagingUpload& upload : m_deferredUploads)
	{
		InFlightStaging entry;
		entry.stagingBuffer = std::move(upload.stagingBuffer);
		entry.fenceValue    = deferredFenceValue;
		m_inFlightStagingBuffers.push_back(std::move(entry));
	}
	m_deferredUploads.clear();

	lastCopyFenceValue = deferredFenceValue;

	// Submit worker lists as batches and track the signaled fence value per queue.
	FrameSyncPoint& sync = m_frameSyncPoints[m_frameIndex];
	sync.copyFenceValue = lastCopyFenceValue;

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

	// Test triangle — drawn until a scene system replaces it.
	if (m_testTriPSO)
	{
		cmd.SetPipelineState(m_testTriPSO.get());
		cmd.SetPrimitiveTopology(PrimitiveTopology::TriangleList);
		cmd.Draw(3);
	}

	// Draw mesh entities from ECS.
	// Each<T...> iterates all entities that have AT LEAST the queried components.
	// Entities with additional components beyond Transform + Mesh are still included.
	if (m_world && m_resourceManager)
	{
		m_world->Each<TransformComponent, MeshComponent>(
			[&cmd, this](Entity entity, TransformComponent& transform, MeshComponent& meshComp)
		{
			if (meshComp.path[0] == '\0')
			{
				return; // No path set
			}

			MeshResource* resource = m_resourceManager->GetMeshResource(meshComp.path);
			if (!resource || resource->state != AssetState::Ready)
			{
				return; // Still loading or uploading — skip this frame
			}

			// TODO: upload transform to per-object constant buffer via m_uploadBuffer
			// TODO: set material / texture bindings per submesh

			cmd.SetVertexBuffer(resource->vertexBuffer.get());
			cmd.SetIndexBuffer(resource->indexBuffer.get());

			for (const Submesh& submesh : resource->mesh->submeshes)
			{
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

void Renderer::QueueCopyForThisFrame(PendingStagingUpload& upload, CommandQueueType queueType)
{
	DYNAMIC_ASSERT(upload.IsValid(), "Renderer::QueueCopyForThisFrame: invalid upload");

	m_urgentUploads.push_back(std::move(upload));

	switch (queueType)
	{
		case CommandQueueType::Graphics: m_graphicsWaitOnCopy = true; break;
		case CommandQueueType::Compute:  m_computeWaitOnCopy  = true; break;
		case CommandQueueType::Copy:     break; // No cross-queue wait needed
	}
}

void Renderer::CreateTestTriangle()
{
	ShaderDesc vsDesc;
	vsDesc.type       = ShaderType::Vertex;
	vsDesc.entryPoint = "VSMain";
	vsDesc.sourceCode = k_triVSSrc;
	m_testTriVS = m_device->CreateShader(vsDesc);

	ShaderDesc psDesc;
	psDesc.type       = ShaderType::Pixel;
	psDesc.entryPoint = "PSMain";
	psDesc.sourceCode = k_triPSSrc;
	m_testTriPS = m_device->CreateShader(psDesc);

	PipelineDesc triDesc;
	triDesc.vertexShader          = m_testTriVS.get();
	triDesc.pixelShader           = m_testTriPS.get();
	triDesc.inputLayout           = {};
	triDesc.renderTargetFormats   = { TextureFormat::BGRA8 };
	triDesc.depthFormat           = TextureFormat::Depth32F;
	triDesc.topology              = PrimitiveTopology::TriangleList;
	triDesc.enableDepthTest       = false;
	triDesc.enableDepthWrite      = false;
	triDesc.enableStencilTest     = false;
	triDesc.enableBlending        = false;
	triDesc.rasterState.cullMode  = RasterizerState::CullMode::None;
	triDesc.rasterState.fillMode  = RasterizerState::FillMode::Solid;
	m_testTriPSO = m_device->CreatePipelineState(triDesc);

	LOG_DEBUG("Renderer: test triangle PSO ready");
}
