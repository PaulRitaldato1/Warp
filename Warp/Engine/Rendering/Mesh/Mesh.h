#pragma once

#include <Common/CommonTypes.h>

// ---------------------------------------------------------------------------
// Vertex — full glTF attribute set.
// Shaders sample only the fields they need; unused fields cost nothing at
// runtime beyond the upload bandwidth, which is a one-time cost.
// ---------------------------------------------------------------------------
struct Vertex
{
	Vec3 position = { 0.f, 0.f, 0.f };
	Vec3 normal   = { 0.f, 1.f, 0.f };

	// xyz = tangent direction, w = bitangent sign (+1 or -1).
	// Bitangent = cross(normal, tangent.xyz) * tangent.w
	Vec4 tangent  = { 1.f, 0.f, 0.f, 1.f };

	Vec2 uv0      = { 0.f, 0.f };  // primary UV (diffuse, PBR maps)
	Vec2 uv1      = { 0.f, 0.f };  // secondary UV (lightmaps)

	// Vertex colour — defaults to opaque white so meshes without colour data
	// render correctly without shader branching.
	Vec4 color    = { 1.f, 1.f, 1.f, 1.f };
};

// ---------------------------------------------------------------------------
// AlphaMode — mirrors glTF AlphaMode.
// ---------------------------------------------------------------------------
enum class AlphaMode : u8
{
	Opaque,  // fully solid, no alpha test
	Mask,    // alpha test against alphaCutoff
	Blend    // blended transparency
};

// ---------------------------------------------------------------------------
// Material — all glTF 2.0 PBR fields plus KHR_materials_unlit.
// Texture indices refer to the Mesh::texturePaths array (-1 = not present).
// Non-PBR shaders simply ignore the fields they don't need.
// ---------------------------------------------------------------------------
struct Material
{
	String name;

	// PBR metallic-roughness
	Vec4  baseColorFactor            = { 1.f, 1.f, 1.f, 1.f };
	f32   metallicFactor             = 1.f;
	f32   roughnessFactor            = 1.f;
	Vec3  emissiveFactor             = { 0.f, 0.f, 0.f };
	f32   alphaCutoff                = 0.5f;

	// Texture slots (-1 = not present)
	int32 baseColorTexture           = -1;
	int32 normalTexture              = -1;
	int32 metallicRoughnessTexture   = -1;
	int32 occlusionTexture           = -1;
	int32 emissiveTexture            = -1;

	AlphaMode alphaMode = AlphaMode::Opaque;
	bool      doubleSided = false;
	bool      unlit       = false;  // KHR_materials_unlit
};

// ---------------------------------------------------------------------------
// Submesh — one draw call's worth of geometry within a Mesh.
// All submeshes share the parent Mesh's vertex and index buffers.
// ---------------------------------------------------------------------------
struct Submesh
{
	u32   indexOffset  = 0;   // first index in Mesh::indices
	u32   indexCount   = 0;
	u32   vertexOffset = 0;   // base vertex added to each index (SV_BaseVertex)
	int32 materialIndex = -1; // into Mesh::materials

	BoundingBox bounds;
};

// ---------------------------------------------------------------------------
// Mesh — CPU-side representation of a loaded glTF asset.
// Upload vertices + indices to GPU buffers once, then retain this for
// CPU-side operations (culling, physics, picking).
// ---------------------------------------------------------------------------
struct Mesh
{
	String name;

	Vector<Vertex>   vertices;
	Vector<u32>      indices;
	Vector<Submesh>  submeshes;
	Vector<Material> materials;

	// Relative paths to image files, as stored in the glTF.
	// Texture indices in Material refer to this array.
	Vector<String>   texturePaths;

	BoundingBox bounds;
};
