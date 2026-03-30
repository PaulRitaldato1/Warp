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

// Per-submesh draw command — everything needed to issue a single DrawIndexed call
// across any pass (geometry, shadow, unlit, etc.).
struct DrawItem
{
	// Transform
	Mat4 model;
	Mat4 modelInvTranspose;

	// GPU buffers (non-owning, valid for the current frame)
	Buffer* vertexBuffer = nullptr;
	Buffer* indexBuffer	 = nullptr;

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
	Vector<DrawItem> items;

	// Sublists — indices into items, built during the same gather pass.
	Vector<u32> shadowCasters;
	Vector<u32> litMeshes;
	Vector<u32> unlitMeshes;

	void Clear()
	{
		items.clear();
		shadowCasters.clear();
		litMeshes.clear();
		unlitMeshes.clear();
	}
};
