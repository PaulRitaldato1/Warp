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

// Matches InstanceData in DeferredGeometry.hlsl and DirectionalShadow.hlsl.
struct InstanceData
{
	Mat4 model;
	Mat4 modelInvTranspose;
	Vec3 boundsCenter;
	u32 batchStart;
	Vec3 boundsExtents;
	u32 batchInfo; // batch count low 16 bits, render flags high 16
};
static_assert(sizeof(InstanceData) == 160, "InstanceData layout must match the shaders");

struct BatchItem
{
	Buffer* positionBuffer	= nullptr;
	Buffer* attributeBuffer = nullptr;
	Buffer* indexBuffer		= nullptr;

	u32 indexCount	 = 0;
	u32 indexOffset	 = 0;
	u32 vertexOffset = 0;

	// Scene batch index, which is also this batch's record in the draw args buffer.
	u32 batchIndex = 0;

	// Where this batch's run starts in the visible index list, and its length.
	// With GPU culling the count is the region size, not the survivors.
	u32 instanceOffset = 0;
	u32 instanceCount  = 0;

	// Material
	Vec3 emissiveFactor						   = { 0.0f, 0.0f, 0.0f };
	Array<Texture*, TextureSlotCount> textures = {};
};

// The draws for one frame, one per batch per pass.
struct DrawList
{
	Vector<BatchItem> batchItems;
	Vector<BatchItem> shadowBatchItems;

	void Clear()
	{
		batchItems.clear();
		shadowBatchItems.clear();
	}
};
