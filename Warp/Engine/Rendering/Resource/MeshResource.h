#pragma once

#include <Common/CommonTypes.h>
#include <Rendering/Resource/ResourceState.h>
#include <Rendering/Mesh/Mesh.h>
#include <Rendering/Renderer/Buffer.h>

struct MeshResource
{
	u32        handle = ~0u;
	AssetState state  = AssetState::Loading;

	// CPU-side data — retained after upload for culling/physics.
	URef<Mesh> mesh;

	// GPU buffers — created once, shared across all entities using this mesh.
	// Position is a separate stream so depth-only passes can bind it alone.
	URef<Buffer> positionBuffer;
	URef<Buffer> attributeBuffer;
	URef<Buffer> indexBuffer;

	// Texture handles — one per Mesh::texturePaths entry, resolved at mesh load time.
	// Index matches the texture indices stored in each Material.
	Vector<u32> textureHandles;
};
