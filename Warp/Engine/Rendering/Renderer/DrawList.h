#pragma once

#include <Common/CommonTypes.h>
#include <Math/Math.h>
#include <Rendering/Mesh/Mesh.h>
#include <Core/ECS/Components/LightComponent.h>

class Buffer;
class Texture;
struct MeshResource;

// Gathered light data — position/direction already resolved from Transform.
struct LightItem
{
	Vec3 position;
	Vec3 direction;
	Vec3 color;
	f32 intensity	   = 1.f;
	f32 range		   = 10.f;
	f32 innerConeAngle = 15.f;
	f32 outerConeAngle = 30.f;
	LightType type	   = LightType::Point;
	bool castShadows   = false;
};

// Built once per frame from the ECS.
struct LightList
{
	Vector<LightItem> items;

	// Sublists — indices into items.
	Vector<u32> shadowCasters;

	void Clear()
	{
		items.clear();
		shadowCasters.clear();
	}
};

struct InstanceSortKey
{
	u64 key;
	u32 instanceIndex;
};

struct InstanceData
{
	Mat4 model;
	Mat4 modelInvTranspose;
	Vec3 boundsCenter;
	f32 pad0;
	Vec3 boundsExtents;
	f32 pad1;
};

struct BatchItem
{
	Buffer* positionBuffer	= nullptr;
	Buffer* attributeBuffer = nullptr;
	Buffer* indexBuffer		= nullptr;

	u32 indexCount	 = 0;
	u32 indexOffset	 = 0;
	u32 vertexOffset = 0;

	// Offset into DrawList::instances that this batch draws
	u32 instanceOffset = 0;
	u32 instanceCount  = 0;

	// Material
	Vec3 emissiveFactor						   = { 0.0f, 0.0f, 0.0f };
	Array<Texture*, TextureSlotCount> textures = {};
};

// Per-submesh draw command — everything needed to issue a single DrawIndexed call
// across any pass (geometry, shadow, unlit, etc.).
struct DrawItem
{
	// Transform
	Mat4 model;
	Mat4 modelInvTranspose;

	// GPU buffers (non-owning, valid for the current frame)
	Buffer* positionBuffer	= nullptr;
	Buffer* attributeBuffer = nullptr;
	Buffer* indexBuffer		= nullptr;

	// Submesh draw range
	u32 indexCount	 = 0;
	u32 indexOffset	 = 0;
	u32 vertexOffset = 0;

	// Material
	Vec3 emissiveFactor						   = { 0.f, 0.f, 0.f };
	Array<Texture*, TextureSlotCount> textures = {};

	// Render flags from MeshComponent (visibility, shadow casting, unlit, etc.)
	u32 renderFlags = 0;
};

// Built once per frame from the ECS in a single pass.
// Sublists store indices into `items` to avoid duplicating DrawItem data.
struct DrawList
{
	Vector<BatchItem> batchItems;
	Vector<BatchItem> shadowBatchItems;

	// Sublists — indices into items, built during the same gather pass.
	Vector<u32> litMeshes;
	Vector<u32> unlitMeshes;

	// Per-entity culling counts, not per-submesh. Correct culling and culling that
	// rejects nothing look identical without these.
	u32 meshesTested = 0;
	u32 meshesCulled = 0;

	void Clear()
	{
		batchItems.clear();
		shadowBatchItems.clear();
		litMeshes.clear();
		unlitMeshes.clear();
		meshesTested = 0;
		meshesCulled = 0;
	}
};
