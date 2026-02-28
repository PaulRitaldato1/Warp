#pragma once

// Abstract resource state used for barrier/transition commands on all backends.
// Backends map these to their own state enums (D3D12_RESOURCE_STATES, VkImageLayout, etc.).
enum class ResourceState
{
	// Initial / don't-care state.  D3D12: COMMON.
	Undefined,

	// Buffer inputs to the fixed-function pipeline.
	VertexBuffer,     // D3D12: VERTEX_AND_CONSTANT_BUFFER
	IndexBuffer,      // D3D12: INDEX_BUFFER
	ConstantBuffer,   // D3D12: VERTEX_AND_CONSTANT_BUFFER (same hardware state as VertexBuffer)

	// Shader-visible read.
	ShaderResource,   // D3D12: PIXEL_SHADER_RESOURCE | NON_PIXEL_SHADER_RESOURCE

	// Shader read/write (compute UAV).
	UnorderedAccess,  // D3D12: UNORDERED_ACCESS

	// Render output.
	RenderTarget,     // D3D12: RENDER_TARGET
	DepthWrite,       // D3D12: DEPTH_WRITE
	DepthRead,        // D3D12: DEPTH_READ   (read-only depth; still bound as DSV)

	// Transfer.
	CopySource,       // D3D12: COPY_SOURCE
	CopyDest,         // D3D12: COPY_DEST

	// Swap-chain back buffer ready for Present().
	Present,          // D3D12: PRESENT
};
