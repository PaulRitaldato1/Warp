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
	URef<Buffer> vertexBuffer;
	URef<Buffer> indexBuffer;

	// Frame counter for tracking upload completion.
	// After k_framesInFlight frames in Uploading state, transitions to Ready.
	u32 uploadFrameCounter = 0;
};
