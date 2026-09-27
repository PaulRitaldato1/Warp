#include <Rendering/Renderer/RenderScene.h>

#include <Core/ECS/Components/MeshComponent.h>
#include <Core/ECS/Components/TransformComponent.h>
#include <Core/ECS/World.h>
#include <Debugging/Assert.h>
#include <Debugging/Profiler.h>
#include <Math/Frustum.h>
#include <Rendering/Mesh/Mesh.h>
#include <Rendering/Resource/MeshResource.h>
#include <Rendering/Resource/ResourceManager.h>

#include <DirectXMath.h>
#include <cmath>

void RenderScene::Reset()
{
	m_instances.clear();
	m_bounds.clear();
	m_slots.clear();
	m_freeHead = k_invalidSlot;
	m_entityToSlot.clear();
	m_batches.clear();
	m_meshToBatchStart.clear();
	m_pendingMeshes.clear();
	m_pendingRetry.clear();
	m_pendingUploads.clear();
	m_bNeedsFullSync = true;
}

RenderScene::SyncStats RenderScene::Sync(World& world, ResourceManager& resources)
{
	PROFILE_SCOPE("SceneSync");

	const Vector<Entity>& removedMeshes		= world.GetRemoved<MeshComponent>();
	const Vector<Entity>& removedTransforms = world.GetRemoved<TransformComponent>();
	const Vector<Entity>& dirtyMeshes		= world.GetDirty<MeshComponent>();
	const Vector<Entity>& dirtyTransforms	= world.GetDirty<TransformComponent>();

	SyncStats stats;
	stats.dirtyTransforms = static_cast<u32>(dirtyTransforms.size());
	stats.dirtyMeshes	  = static_cast<u32>(dirtyMeshes.size());
	stats.removed		  = static_cast<u32>(removedMeshes.size());

	// Removals first. A recycled id is freed under its old owner here, then
	// picked up under its new owner by the dirty lists below.
	for (Entity entity : removedMeshes)
	{
		FreeSlot(entity);
	}
	for (Entity entity : removedTransforms)
	{
		FreeSlot(entity);
	}

	// Entities created before tracking started were never marked.
	if (m_bNeedsFullSync)
	{
		m_bNeedsFullSync = false;
		world.Each<const TransformComponent, const MeshComponent>(
			[&](Entity entity, const TransformComponent&, const MeshComponent&)
			{ UpdateMesh(world, resources, entity); });
	}

	// Swapped out first since UpdateMesh parks anything still loading again.
	std::swap(m_pendingMeshes, m_pendingRetry);
	for (Entity entity : m_pendingRetry)
	{
		if (!world.IsAlive(entity) || m_entityToSlot[entity.id] != k_pendingSlot)
		{
			continue; // Destroyed, or resolved some other way since.
		}

		m_entityToSlot[entity.id] = k_invalidSlot;
		UpdateMesh(world, resources, entity);
	}
	m_pendingRetry.clear();

	for (Entity entity : dirtyMeshes)
	{
		UpdateMesh(world, resources, entity);
	}

	for (Entity entity : dirtyTransforms)
	{
		UpdateTransform(world, resources, entity);
	}

	world.ClearRemoved<MeshComponent>();
	world.ClearRemoved<TransformComponent>();
	world.ClearDirty<MeshComponent>();
	world.ClearDirty<TransformComponent>();

	return stats;
}

void RenderScene::Cull(const Array<Vec4, 6>& frustum, u32 requiredFlags, CullResult& out) const
{
	PROFILE_SCOPE("Cull");

	// Free slots carry no flags, so zero here would draw them.
	DYNAMIC_ASSERT(requiredFlags != 0, "RenderScene::Cull: requiredFlags must be non-zero");

	out.indices.clear();
	out.batches.clear();
	out.tested = 0;
	out.culled = 0;

	out.perBatch.resize(m_batches.size());
	for (Vector<u32>& batchIndices : out.perBatch)
	{
		batchIndices.clear();
	}

	// Walking slots in order keeps each batch's indices ascending, so the vertex
	// shader reads the instance buffer front to back.
	const u32 slotCount = static_cast<u32>(m_slots.size());
	for (u32 slot = 0; slot < slotCount; ++slot)
	{
		const SlotInfo& info = m_slots[slot];
		if ((info.renderFlags & requiredFlags) != requiredFlags)
		{
			continue;
		}

		++out.tested;
		if (!IsVisible(frustum, m_bounds[slot]))
		{
			++out.culled;
			continue;
		}

		// Tested once, emitted into every submesh's batch.
		for (u32 batch = info.batchStart; batch < info.batchStart + info.batchCount; ++batch)
		{
			if (m_batches[batch].bDrawable)
			{
				out.perBatch[batch].push_back(slot);
			}
		}
	}

	for (u32 batch = 0; batch < static_cast<u32>(out.perBatch.size()); ++batch)
	{
		const Vector<u32>& batchIndices = out.perBatch[batch];
		if (batchIndices.empty())
		{
			continue;
		}

		out.batches.push_back({ batch, static_cast<u32>(out.indices.size()), static_cast<u32>(batchIndices.size()) });
		out.indices.insert(out.indices.end(), batchIndices.begin(), batchIndices.end());
	}
}

RenderScene::CullRegions RenderScene::ComputeCullRegions() const
{
	CullRegions regions;
	regions.regionStarts.reserve(m_batches.size());

	for (u32 batch = 0; batch < static_cast<u32>(m_batches.size()); ++batch)
	{
		const Batch& info = m_batches[batch];
		regions.regionStarts.push_back(regions.visibleListSize);

		if (info.bDrawable && info.memberCount > 0)
		{
			regions.drawableRegions.push_back({ batch, regions.visibleListSize, info.memberCount });
		}

		regions.visibleListSize += info.memberCount;
	}

	return regions;
}

void RenderScene::ClearUploads()
{
	for (u32 slot : m_pendingUploads)
	{
		m_slots[slot].bQueuedForUpload = false;
	}
	m_pendingUploads.clear();
}

void RenderScene::UpdateMesh(World& world, ResourceManager& resources, Entity entity)
{
	// HasComponent is false for dead entities too.
	if (!world.HasComponent<MeshComponent>(entity) || !world.HasComponent<TransformComponent>(entity))
	{
		FreeSlot(entity);
		return;
	}

	const MeshComponent& mesh = world.Read<MeshComponent>(entity);
	if (!mesh.IsHandleValid())
	{
		FreeSlot(entity);
		return;
	}

	const MeshResource* resource = resources.GetMeshResourceByHandle(mesh.meshHandle);
	if (!resource)
	{
		u32& ref = EntitySlotRef(entity);
		if (ref == k_pendingSlot)
		{
			return; // Already parked.
		}

		// Stops drawing the old mesh until the new one is ready.
		FreeSlot(entity);
		ref = k_pendingSlot;
		m_pendingMeshes.push_back(entity);
		return;
	}

	const u32 batchStart = GetOrCreateBatches(mesh.meshHandle, *resource);
	const u32 batchCount = static_cast<u32>(resource->mesh->submeshes.size());

	u32 slot = SlotOf(entity);
	if (slot == k_invalidSlot)
	{
		slot = AllocateSlot(entity);
	}

	SlotInfo& info = m_slots[slot];
	if (info.batchStart != batchStart || info.batchCount != batchCount)
	{
		SetBatches(slot, batchStart, batchCount);
	}
	info.meshHandle	 = mesh.meshHandle;
	info.renderFlags = mesh.renderFlags;

	// Bounds come from the mesh, so a mesh change needs the instance rebuilt too.
	WriteInstance(slot, world.Read<TransformComponent>(entity), *resource);
}

void RenderScene::UpdateTransform(World& world, ResourceManager& resources, Entity entity)
{
	// No slot means no mesh yet, or not renderable. The camera lands here every frame.
	const u32 slot = SlotOf(entity);
	if (slot == k_invalidSlot || !world.IsAlive(entity))
	{
		return;
	}

	const MeshResource* resource = resources.GetMeshResourceByHandle(m_slots[slot].meshHandle);
	if (!resource)
	{
		return;
	}

	WriteInstance(slot, world.Read<TransformComponent>(entity), *resource);
}

void RenderScene::WriteInstance(u32 slot, const TransformComponent& transform, const MeshResource& resource)
{
	using namespace DirectX;

	InstanceData& instance = m_instances[slot];
	const SlotInfo& info   = m_slots[slot];

	SimdMat S = XMMatrixScaling(transform.scale.x, transform.scale.y, transform.scale.z);
	SimdMat R = XMMatrixRotationQuaternion(XMLoadFloat4(&transform.rotation));
	SimdMat T = XMMatrixTranslation(transform.position.x, transform.position.y, transform.position.z);
	SimdMat M = XMMatrixMultiply(XMMatrixMultiply(S, R), T);
	XMStoreFloat4x4(&instance.model, M);

	// Under uniform scale the inverse transpose is the rotation times 1/s, and
	// the shader normalizes, so M itself gives the same normal. Only non-uniform
	// scale needs the inverse, which is the expensive path.
	constexpr f32 k_scaleEpsilon = 1e-5f;
	const bool uniformScale		 = fabsf(transform.scale.x - transform.scale.y) < k_scaleEpsilon &&
							   fabsf(transform.scale.y - transform.scale.z) < k_scaleEpsilon;
	XMStoreFloat4x4(&instance.modelInvTranspose, uniformScale ? M : XMMatrixTranspose(XMMatrixInverse(nullptr, M)));

	// Refitting an AABB after rotation grows it, which costs some false positives.
	BoundingBox& bounds = m_bounds[slot];
	resource.mesh->bounds.Transform(bounds, M);
	instance.boundsCenter  = bounds.Center;
	instance.boundsExtents = bounds.Extents;

	// Not read by any shader yet.
	instance.batchStart = info.batchStart;
	instance.batchInfo	= (info.batchCount & 0xFFFF) | (info.renderFlags << 16);

	QueueUpload(slot);
}

u32 RenderScene::SlotOf(Entity entity) const
{
	if (entity.id >= m_entityToSlot.size())
	{
		return k_invalidSlot;
	}

	const u32 slot = m_entityToSlot[entity.id];
	if (slot == k_invalidSlot || slot == k_pendingSlot)
	{
		return k_invalidSlot;
	}

	// Generation check, so a recycled id never reaches its predecessor's slot.
	return m_slots[slot].owner == entity ? slot : k_invalidSlot;
}

u32 RenderScene::AllocateSlot(Entity entity)
{
	u32 slot;
	if (m_freeHead != k_invalidSlot)
	{
		slot				   = m_freeHead;
		m_freeHead			   = m_slots[slot].batchStart;
		m_slots[slot].batchStart = 0;
	}
	else
	{
		slot = static_cast<u32>(m_slots.size());
		m_instances.emplace_back();
		m_bounds.emplace_back();
		m_slots.emplace_back();
	}

	m_slots[slot].owner = entity;
	EntitySlotRef(entity) = slot;
	return slot;
}

void RenderScene::FreeSlot(Entity entity)
{
	if (entity.id >= m_entityToSlot.size())
	{
		return;
	}

	u32& ref = m_entityToSlot[entity.id];
	if (ref == k_pendingSlot)
	{
		// The stale pending list entry is skipped on retry.
		ref = k_invalidSlot;
		return;
	}

	if (ref == k_invalidSlot || m_slots[ref].owner != entity)
	{
		return; // Already freed, or a duplicate removal.
	}

	const u32 slot = ref;
	SetBatches(slot, 0, 0);

	// The GPU cull reads flags from the instance itself, so a freed slot has to
	// be cleared there too or it keeps drawing.
	m_instances[slot].batchStart = 0;
	m_instances[slot].batchInfo	 = 0;
	QueueUpload(slot);

	// Keep the upload flag, since the slot is now in the pending uploads list.
	SlotInfo& info				= m_slots[slot];
	const bool bQueuedForUpload = info.bQueuedForUpload;
	info						= SlotInfo{};
	info.bQueuedForUpload		= bQueuedForUpload;
	info.batchStart				= m_freeHead;
	m_freeHead					= slot;

	ref = k_invalidSlot;
}

void RenderScene::SetBatches(u32 slot, u32 batchStart, u32 batchCount)
{
	SlotInfo& info = m_slots[slot];

	for (u32 batch = info.batchStart; batch < info.batchStart + info.batchCount; ++batch)
	{
		--m_batches[batch].memberCount;
	}

	info.batchStart = batchStart;
	info.batchCount = batchCount;

	for (u32 batch = batchStart; batch < batchStart + batchCount; ++batch)
	{
		++m_batches[batch].memberCount;
	}
}

u32 RenderScene::GetOrCreateBatches(u32 meshHandle, const MeshResource& resource)
{
	auto it = m_meshToBatchStart.find(meshHandle);
	if (it != m_meshToBatchStart.end())
	{
		return it->second;
	}

	const u32 batchStart = static_cast<u32>(m_batches.size());
	const Vector<Submesh>& submeshes = resource.mesh->submeshes;

	for (u32 submeshIndex = 0; submeshIndex < static_cast<u32>(submeshes.size()); ++submeshIndex)
	{
		Batch batch;
		batch.key		= (static_cast<u64>(meshHandle) << 32) | submeshIndex;
		batch.bDrawable = submeshes[submeshIndex].materialIndex >= 0;
		m_batches.push_back(batch);
	}

	m_meshToBatchStart.emplace(meshHandle, batchStart);
	return batchStart;
}

void RenderScene::QueueUpload(u32 slot)
{
	SlotInfo& info = m_slots[slot];
	if (!info.bQueuedForUpload)
	{
		info.bQueuedForUpload = true;
		m_pendingUploads.push_back(slot);
	}
}

u32& RenderScene::EntitySlotRef(Entity entity)
{
	if (entity.id >= m_entityToSlot.size())
	{
		m_entityToSlot.resize(entity.id + 1, k_invalidSlot);
	}
	return m_entityToSlot[entity.id];
}
