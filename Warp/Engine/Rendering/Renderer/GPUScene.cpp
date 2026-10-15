#include <Core/Console/ConsoleVariable.h>
#include <Core/ECS/Components/MeshComponent.h>
#include <Core/ECS/Components/TransformComponent.h>
#include <Core/ECS/World.h>
#include <Debugging/Assert.h>
#include <Debugging/Logging.h>
#include <Debugging/Profiler.h>
#include <Rendering/Mesh/Mesh.h>
#include <Rendering/Passes/CullPass.h>
#include <Rendering/Renderer/Buffer.h>
#include <Rendering/Renderer/Device.h>
#include <Rendering/Renderer/FrameBlackboard.h>
#include <Rendering/Renderer/FrameView.h>
#include <Rendering/Renderer/GPUScene.h>
#include <Rendering/Renderer/RenderGraph.h>
#include <Rendering/Renderer/UploadBuffer.h>
#include <Rendering/Resource/MeshResource.h>
#include <Rendering/Resource/ResourceManager.h>
#include <Rendering/Resource/TextureResource.h>

#include <algorithm>

// Static: only this file reads it. Other code goes through SetGPUCulling, or
// through the registry by name, which is how the console reaches it.
static Cvar<bool> CvarGPUCulling("r.GPUCulling", true,
								 "Cull instances on the GPU. Off uses the CPU cull, kept as a reference");

GPUScene::~GPUScene() = default;

void GPUScene::Init(Device* device, PipelineLibrary& pipelines, u32 framesInFlight)
{
	DYNAMIC_ASSERT(device, "GPUScene::Init: device is null");

	m_device	   = device;
	m_cullPipeline = CreateCullPassPipeline(pipelines);

	m_retiredBuffers.resize(framesInFlight);
	for (Vector<ArgsReadback>& readbacks : m_argsReadback)
	{
		readbacks.resize(framesInFlight);
	}

	// Owned here, unsubscribed in Shutdown: the cvar outlives the scene.
	m_gpuCullingChangedDelegate =
		std::make_unique<MemberFuncType<GPUScene, bool>>(this, &GPUScene::OnGPUCullingChanged);
	CvarGPUCulling.SubscribeToChanged(m_gpuCullingChangedDelegate.get());
}

void GPUScene::Shutdown()
{
	if (m_gpuCullingChangedDelegate)
	{
		CvarGPUCulling.UnsubscribeFromChanged(m_gpuCullingChangedDelegate.get());
		m_gpuCullingChangedDelegate.reset();
	}
}

void GPUScene::SetWorld(World* world)
{
	m_world = world;
	m_scene.Reset();
	if (m_world)
	{
		m_world->TrackChanges<TransformComponent>();
		m_world->TrackChanges<MeshComponent>();
	}
}

void GPUScene::SetResourceManager(ResourceManager* resourceManager)
{
	m_resourceManager = resourceManager;
}

void GPUScene::SetGPUCulling(bool bEnabled)
{
	CvarGPUCulling.Set(bEnabled);
}

bool GPUScene::IsGPUCulling() const
{
	return CvarGPUCulling.Get();
}

void GPUScene::OnGPUCullingChanged(bool bEnabled)
{
	// Nothing to rebuild: the next frame picks a path from the cvar. This is where
	// a cvar that needs work on change, like vsync, would do it.
	LOG_DEBUG("GPUScene: r.GPUCulling = {}, culling on the {}", bEnabled, bEnabled ? "GPU" : "CPU");
}

void GPUScene::BeginFrame(u32 frameIndex)
{
	m_frameIndex = frameIndex;

	// Safe now that this slot's fence has been waited on.
	m_retiredBuffers[m_frameIndex].clear();
	m_readbackStats = ReadArgsReadback();
}

void GPUScene::AddUploadAndCull(RenderGraph& graph, FrameBlackboard& blackboard, const FrameView& view,
								const PassContext& context)
{
	DYNAMIC_ASSERT(m_world && m_resourceManager, "GPUScene::AddUploadAndCull: needs a world and a resource manager");

	m_stats					 = {};
	m_stats.numTris			 = m_readbackStats.numTris;
	m_stats.visibleInstances = m_readbackStats.visibleInstances;

	// ---------------------------------------------------------------------------
	// Sync persistent instance data from the ECS, then cull it per view. Sync only
	// touches entities that changed; the cull walks every slot but reads only
	// cached bounds.
	// ---------------------------------------------------------------------------

	const RenderScene::SyncStats syncStats = m_scene.Sync(*m_world, *m_resourceManager);
	m_stats.dirtyTransforms				   = syncStats.dirtyTransforms;
	m_stats.dirtyMeshes					   = syncStats.dirtyMeshes;
	m_stats.removedMeshes				   = syncStats.removed;

	// Shadow casters are tested against the sun instead, since geometry outside
	// the view can still cast into it.
	const u32 cameraFlags = RenderFlags_Visible;
	const u32 sunFlags	  = static_cast<u32>(RenderFlags_Visible | RenderFlags_CastShadow);
	m_bHasSunView		  = view.sky.bHasSun;

	// Both culls end up in the same shape: args indexed by batch, and per batch
	// an offset and count into a visible index list.
	const RenderScene::CullRegions regions = m_scene.ComputeCullRegions();
	Vector<DrawIndexedArgs> cameraArgs	   = BuildDrawArgs();
	Vector<DrawIndexedArgs> sunArgs		   = cameraArgs;

	// Read once: the cull path, the args buffer state and the dispatch below must
	// agree, even if the console changes the cvar partway through the frame.
	const bool bGPUCulling = CvarGPUCulling.Get();

	if (bGPUCulling)
	{
		// The CPU no longer knows which batches have survivors, so every batch with
		// members is drawn. One with none becomes a zero instance draw.
		m_drawList.batchItems		= BuildBatchItems(regions.occupiedRegions);
		m_drawList.shadowBatchItems = m_bHasSunView ? m_drawList.batchItems : Vector<BatchItem>{};
	}
	else
	{
		m_scene.Cull(view.camera.frustum, cameraFlags, m_cameraCull);

		if (m_bHasSunView)
		{
			m_scene.Cull(view.sky.sunFrustum, sunFlags, m_sunCull);
		}
		else
		{
			m_sunCull.indices.clear();
			m_sunCull.batches.clear();
		}

		m_stats.tested				= m_cameraCull.tested;
		m_stats.culled				= m_cameraCull.culled;
		m_drawList.batchItems		= BuildBatchItems(m_cameraCull.batches);
		m_drawList.shadowBatchItems = BuildBatchItems(m_sunCull.batches);

		for (const RenderScene::VisibleBatch& visible : m_cameraCull.batches)
		{
			cameraArgs[visible.batchIndex].instanceCount = visible.count;
		}
		for (const RenderScene::VisibleBatch& visible : m_sunCull.batches)
		{
			sunArgs[visible.batchIndex].instanceCount = visible.count;
		}
	}

	m_stats.batches = static_cast<u32>(m_drawList.batchItems.size());

	// Nothing to cull when no batch has members.
	const bool bRunGPUCull	   = bGPUCulling && regions.visibleListSize > 0;
	const bool bInstancesGrown = GrowBuffers(regions, bGPUCulling);

	graph.AddPass("Scene Upload")
		.Execute(
			[this, context, bInstancesGrown, bGPUCulling,
			 regionStarts = bRunGPUCull ? regions.regionStarts : Vector<u32>{}, cameraArgs = std::move(cameraArgs),
			 sunArgs = std::move(sunArgs)](CommandList& cmd)
			{
				UploadBuffer& uploadBuffer = *context.uploadBuffer;

				// Both draw passes read the same instance buffer, so it is written once up front.
				UploadInstances(cmd, uploadBuffer, bInstancesGrown);

				// The GPU cull counts into the args, so they stay writable until it is done.
				const ResourceState argsState =
					bGPUCulling ? ResourceState::UnorderedAccess : ResourceState::IndirectArgument;
				UploadDrawArgs(cmd, uploadBuffer, cameraArgs, m_cameraArgsBuffer.get(), argsState);
				UploadDrawArgs(cmd, uploadBuffer, sunArgs, m_sunArgsBuffer.get(), argsState);

				if (bGPUCulling)
				{
					// The cull's input. It writes the visible indices itself.
					UploadSlotIndices(cmd, uploadBuffer, regionStarts, m_regionStartBuffer.get());
				}
				else
				{
					// The CPU cull already knows the visible indices.
					UploadSlotIndices(cmd, uploadBuffer, m_cameraCull.indices, m_cameraIndexBuffer.get());
					UploadSlotIndices(cmd, uploadBuffer, m_sunCull.indices, m_sunIndexBuffer.get());
				}
			});

	if (bRunGPUCull)
	{
		CullPassInputs cull;
		cull.pipeline	  = m_cullPipeline;
		cull.instances	  = m_instanceBuffer.get();
		cull.regionStarts = m_regionStartBuffer.get();
		cull.slotCount	  = static_cast<u32>(m_scene.GetInstances().size());
		cull.views.push_back({ view.camera.frustum, cameraFlags, m_cameraArgsBuffer.get(), m_cameraIndexBuffer.get() });

		if (m_bHasSunView)
		{
			cull.views.push_back({ view.sky.sunFrustum, sunFlags, m_sunArgsBuffer.get(), m_sunIndexBuffer.get() });
		}

		AddCullPass(graph, context, std::move(cull));
	}

	GPUSceneOutput output;
	output.camera	 = { m_drawList.batchItems, m_instanceBuffer.get(), m_cameraIndexBuffer.get(),
						 m_cameraArgsBuffer.get() };
	output.sunShadow = { m_drawList.shadowBatchItems, m_instanceBuffer.get(), m_sunIndexBuffer.get(),
						 m_sunArgsBuffer.get() };
	blackboard.Add(output);
}

void GPUScene::AddReadback(RenderGraph& graph)
{
	// Only the GPU knows the final instance counts, so they come back for the stats.
	graph.AddPass("Args Readback")
		.Execute(
			[this](CommandList& cmd)
			{
				const u32 batchCount = m_scene.GetBatchCount();
				RecordArgsReadback(cmd, ReadbackView::Camera, m_cameraArgsBuffer.get(), batchCount);
				RecordArgsReadback(cmd, ReadbackView::Sun, m_bHasSunView ? m_sunArgsBuffer.get() : nullptr, batchCount);
			});
}

bool GPUScene::EnsureBufferCapacity(URef<Buffer>& buffer, u32& capacity, u32 needed, u32 stride, const char* name,
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

	LOG_DEBUG("GPUScene: {} grown to {} elements", name, newCapacity);
	return true;
}

bool GPUScene::GrowBuffers(const RenderScene::CullRegions& regions, bool bGPUCulling)
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
		EnsureBufferCapacity(m_cameraArgsBuffer, m_cameraArgsCapacity, batchCount, sizeof(DrawIndexedArgs),
							 "CameraArgsBuffer", true);
		EnsureBufferCapacity(m_sunArgsBuffer, m_sunArgsCapacity, batchCount, sizeof(DrawIndexedArgs), "SunArgsBuffer",
							 true);
	}

	if (bGPUCulling)
	{
		if (regions.visibleListSize > 0)
		{
			EnsureBufferCapacity(m_regionStartBuffer, m_regionStartCapacity,
								 static_cast<u32>(regions.regionStarts.size()), sizeof(u32), "RegionStartBuffer");

			// Sized for the worst case, every member visible. The cull fills a prefix
			// of each batch's region and the rest is never read.
			EnsureBufferCapacity(m_cameraIndexBuffer, m_cameraIndexCapacity, regions.visibleListSize, sizeof(u32),
								 "CameraIndexBuffer", true);
			EnsureBufferCapacity(m_sunIndexBuffer, m_sunIndexCapacity, regions.visibleListSize, sizeof(u32),
								 "SunIndexBuffer", true);
		}
	}
	else
	{
		if (!m_cameraCull.indices.empty())
		{
			EnsureBufferCapacity(m_cameraIndexBuffer, m_cameraIndexCapacity,
								 static_cast<u32>(m_cameraCull.indices.size()), sizeof(u32), "CameraIndexBuffer", true);
		}
		if (!m_sunCull.indices.empty())
		{
			EnsureBufferCapacity(m_sunIndexBuffer, m_sunIndexCapacity, static_cast<u32>(m_sunCull.indices.size()),
								 sizeof(u32), "SunIndexBuffer", true);
		}
	}

	return bInstancesGrown;
}

Vector<DrawIndexedArgs> GPUScene::BuildDrawArgs() const
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

Vector<BatchItem> GPUScene::BuildBatchItems(const Vector<RenderScene::VisibleBatch>& batches) const
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
		FATAL_ASSERT(resource, "GPUScene::BuildBatchItems: MeshResource is invalid for a live batch");

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

void GPUScene::UploadInstances(CommandList& cmd, UploadBuffer& uploadBuffer, bool bFullUpload)
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

	DYNAMIC_ASSERT(m_instanceCapacity >= slotCount, "GPUScene::UploadInstances: grow it in GrowBuffers first");

	// Past a quarter dirty, one big copy beats many small ones.
	const bool bCopyAll = bFullUpload || pending.size() * 4 > slotCount;
	if (!bCopyAll && pending.empty())
	{
		return;
	}

	Buffer* staging = uploadBuffer.GetBackingBuffer();
	cmd.TransitionBuffer(m_instanceBuffer.get(), ResourceState::CopyDest);

	if (bCopyAll)
	{
		const u64 bytes		= static_cast<u64>(slotCount) * sizeof(InstanceData);
		UploadResult staged = uploadBuffer.AllocAndCopy(instances.data(), bytes);
		cmd.CopyBuffer(staging, m_instanceBuffer.get(), staged.offset, 0, bytes);
		m_stats.uploadedSlots = slotCount;
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
			UploadResult staged = uploadBuffer.AllocAndCopy(&instances[firstSlot], bytes);
			cmd.CopyBuffer(staging, m_instanceBuffer.get(), staged.offset,
						   static_cast<u64>(firstSlot) * sizeof(InstanceData), bytes);

			runStart = runEnd;
		}
		m_stats.uploadedSlots = static_cast<u32>(pending.size());
	}

	cmd.TransitionBuffer(m_instanceBuffer.get(), ResourceState::ShaderResource);
	m_scene.ClearUploads();
}

void GPUScene::UploadSlotIndices(CommandList& cmd, UploadBuffer& uploadBuffer, const Vector<u32>& indices,
								 Buffer* buffer)
{
	if (indices.empty())
	{
		return;
	}

	const u64 bytes = static_cast<u64>(indices.size()) * sizeof(u32);
	DYNAMIC_ASSERT(buffer && buffer->GetSize() >= bytes, "GPUScene::UploadSlotIndices: grow it in GrowBuffers first");

	UploadResult staged = uploadBuffer.AllocAndCopy(indices.data(), bytes);

	cmd.TransitionBuffer(buffer, ResourceState::CopyDest);
	cmd.CopyBuffer(uploadBuffer.GetBackingBuffer(), buffer, staged.offset, 0, bytes);
	cmd.TransitionBuffer(buffer, ResourceState::ShaderResource);
}

void GPUScene::UploadDrawArgs(CommandList& cmd, UploadBuffer& uploadBuffer, const Vector<DrawIndexedArgs>& args,
							  Buffer* buffer, ResourceState finalState)
{
	if (args.empty())
	{
		return;
	}

	const u64 bytes = static_cast<u64>(args.size()) * sizeof(DrawIndexedArgs);
	DYNAMIC_ASSERT(buffer && buffer->GetSize() >= bytes, "GPUScene::UploadDrawArgs: grow it in GrowBuffers first");

	UploadResult staged = uploadBuffer.AllocAndCopy(args.data(), bytes);

	// Also resets last frame's GPU counts, since every record is rewritten.
	cmd.TransitionBuffer(buffer, ResourceState::CopyDest);
	cmd.CopyBuffer(uploadBuffer.GetBackingBuffer(), buffer, staged.offset, 0, bytes);
	cmd.TransitionBuffer(buffer, finalState);
}

void GPUScene::RecordArgsReadback(CommandList& cmd, ReadbackView readbackView, Buffer* drawArgs, u32 count)
{
	ArgsReadback& readback = m_argsReadback[static_cast<u32>(readbackView)][m_frameIndex];
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
		desc.name		 = readbackView == ReadbackView::Camera ? "CameraArgsReadback" : "SunArgsReadback";
		readback.buffer	 = m_device->CreateBuffer(desc);
	}

	cmd.TransitionBuffer(drawArgs, ResourceState::CopySource);
	cmd.CopyBuffer(drawArgs, readback.buffer.get(), 0, 0, bytes);
	readback.count = count;
}

GPUScene::ReadbackStats GPUScene::ReadArgsReadback()
{
	ReadbackStats stats;

	for (u32 viewIndex = 0; viewIndex < static_cast<u32>(ReadbackView::Count); ++viewIndex)
	{
		const ArgsReadback& readback = m_argsReadback[viewIndex][m_frameIndex];
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
			if (viewIndex == static_cast<u32>(ReadbackView::Camera))
			{
				stats.visibleInstances += args[batch].instanceCount;
			}
		}
		readback.buffer->Unmap();
	}

	return stats;
}
