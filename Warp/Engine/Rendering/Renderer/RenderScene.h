#pragma once

#include <Common/CommonTypes.h>
#include <Core/ECS/Entity.h>
#include <Math/Math.h>
#include <Rendering/Renderer/DrawList.h>

class World;
class ResourceManager;
struct MeshResource;
struct TransformComponent;

// Persistent per-entity instance data. Built once and rebuilt only for entities
// whose transform or mesh changed, so a static scene costs nothing here after
// the first frame. Only culling still walks every slot.
class RenderScene
{
public:
	static constexpr u32 k_invalidSlot = ~0u;

	// One per (mesh, submesh). A mesh's batches are allocated consecutively, so
	// a slot names all of its batches with batchStart and batchCount.
	struct Batch
	{
		u64 key			   = 0; // (meshHandle << 32) | submeshIndex
		u32 memberCount	   = 0; // live slots in this batch, Phase 6's region size
		bool bDrawable	   = true;
	};

	// One contiguous run of slot indices per batch that has survivors.
	struct VisibleBatch
	{
		u32 batchIndex = 0;
		u32 offset	   = 0;
		u32 count	   = 0;
	};

	struct CullResult
	{
		Vector<u32> indices;
		Vector<VisibleBatch> batches;
		u32 tested = 0;
		u32 culled = 0;

		// Per batch scratch, kept to reuse capacity across frames.
		Vector<Vector<u32>> perBatch;
	};

	struct SyncStats
	{
		u32 dirtyTransforms = 0;
		u32 dirtyMeshes		= 0;
		u32 removed			= 0;
	};

	// Drops everything and repopulates from the world on the next Sync.
	void Reset();

	// Drains the world's change lists into slot updates.
	SyncStats Sync(World& world, ResourceManager& resources);

	// Tests every live slot carrying all of requiredFlags against frustum.
	void Cull(const Array<Vec4, 6>& frustum, u32 requiredFlags, CullResult& out) const;

	const Vector<InstanceData>& GetInstances() const
	{
		return m_instances;
	}

	const Batch& GetBatch(u32 batchIndex) const
	{
		return m_batches[batchIndex];
	}

	// Slots written since the last ClearUploads, unsorted and unique.
	Vector<u32>& GetPendingUploads()
	{
		return m_pendingUploads;
	}

	void ClearUploads();

private:
	struct SlotInfo
	{
		Entity owner	= k_nullEntity;
		u32 meshHandle	= ~0u;
		u32 batchStart	= 0; // next free slot while on the free list
		u32 batchCount	= 0;
		u32 renderFlags = 0; // 0 while free, which the cull loop skips
		bool bQueuedForUpload = false;
	};

	// Parked in entityToSlot while the mesh is still loading.
	static constexpr u32 k_pendingSlot = ~0u - 1;

	void UpdateMesh(World& world, ResourceManager& resources, Entity entity);
	void UpdateTransform(World& world, ResourceManager& resources, Entity entity);
	void WriteInstance(u32 slot, const TransformComponent& transform, const MeshResource& resource);

	u32 SlotOf(Entity entity) const;
	u32 AllocateSlot(Entity entity);
	void FreeSlot(Entity entity);
	void SetBatches(u32 slot, u32 batchStart, u32 batchCount);
	u32 GetOrCreateBatches(u32 meshHandle, const MeshResource& resource);
	void QueueUpload(u32 slot);

	u32& EntitySlotRef(Entity entity);

	// Indexed by slot. m_instances is the CPU mirror of the GPU instance buffer.
	Vector<InstanceData> m_instances;
	Vector<BoundingBox> m_bounds;
	Vector<SlotInfo> m_slots;
	u32 m_freeHead = k_invalidSlot;

	// Indexed by entity.id.
	Vector<u32> m_entityToSlot;

	Vector<Batch> m_batches;
	HashMap<u32, u32> m_meshToBatchStart;

	Vector<Entity> m_pendingMeshes;
	Vector<Entity> m_pendingRetry;
	Vector<u32> m_pendingUploads;

	bool m_bNeedsFullSync = true;
};
