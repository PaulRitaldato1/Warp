#include <Rendering/Mesh/MeshLoader.h>
#include <Debugging/Logging.h>
#include <Threading/ThreadPool.h>

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/dxmath_element_traits.hpp>

#include <filesystem>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static AlphaMode TranslateAlphaMode(fastgltf::AlphaMode mode)
{
	switch (mode)
	{
		case fastgltf::AlphaMode::Mask:  return AlphaMode::Mask;
		case fastgltf::AlphaMode::Blend: return AlphaMode::Blend;
		default:                         return AlphaMode::Opaque;
	}
}

// ---------------------------------------------------------------------------
// Material extraction
// ---------------------------------------------------------------------------

static void ExtractMaterials(const fastgltf::Asset& asset, Mesh& out)
{
	out.materials.reserve(asset.materials.size());

	for (const auto& src : asset.materials)
	{
		Material mat;
		mat.name        = src.name;
		mat.doubleSided = src.doubleSided;
		mat.alphaCutoff = src.alphaCutoff;
		mat.alphaMode   = TranslateAlphaMode(src.alphaMode);
		mat.unlit       = src.unlit;

		const auto& pbr       = src.pbrData;
		mat.baseColorFactor   = Vec4(pbr.baseColorFactor[0], pbr.baseColorFactor[1],
									 pbr.baseColorFactor[2], pbr.baseColorFactor[3]);
		mat.metallicFactor    = pbr.metallicFactor;
		mat.roughnessFactor   = pbr.roughnessFactor;
		mat.emissiveFactor    = Vec3(src.emissiveFactor[0], src.emissiveFactor[1],
									 src.emissiveFactor[2]);

		if (pbr.baseColorTexture.has_value())
		{
			mat.baseColorTexture = static_cast<int32>(pbr.baseColorTexture->textureIndex);
		}
		if (pbr.metallicRoughnessTexture.has_value())
		{
			mat.metallicRoughnessTexture = static_cast<int32>(pbr.metallicRoughnessTexture->textureIndex);
		}
		if (src.normalTexture.has_value())
		{
			mat.normalTexture = static_cast<int32>(src.normalTexture->textureIndex);
		}
		if (src.occlusionTexture.has_value())
		{
			mat.occlusionTexture = static_cast<int32>(src.occlusionTexture->textureIndex);
		}
		if (src.emissiveTexture.has_value())
		{
			mat.emissiveTexture = static_cast<int32>(src.emissiveTexture->textureIndex);
		}

		out.materials.push_back(std::move(mat));
	}
}

// ---------------------------------------------------------------------------
// Texture path extraction
// ---------------------------------------------------------------------------

static void ExtractTexturePaths(const fastgltf::Asset& asset, Mesh& out)
{
	out.texturePaths.reserve(asset.textures.size());

	for (const auto& tex : asset.textures)
	{
		if (!tex.imageIndex.has_value())
		{
			out.texturePaths.emplace_back("");
			continue;
		}

		const auto& image = asset.images[tex.imageIndex.value()];
		String path;

		std::visit(fastgltf::visitor{
			[&](const fastgltf::sources::URI& uri)
			{
				path = String(uri.uri.path().begin(), uri.uri.path().end());
			},
			[&](const fastgltf::sources::BufferView&) { /* embedded — handled at load time */ },
			[](auto&) {}
		}, image.data);

		out.texturePaths.push_back(std::move(path));
	}
}

// ---------------------------------------------------------------------------
// Geometry extraction — one primitive at a time
// ---------------------------------------------------------------------------

template <typename T>
static void CopyAttribute(const fastgltf::Asset& asset,
						   const fastgltf::Primitive& prim,
						   std::string_view name,
						   Vector<Vertex>& vertices,
						   u32 vertexBase,
						   T Vertex::* member)
{
	auto it = prim.findAttribute(name);
	if (it == prim.attributes.end()) { return; }

	fastgltf::iterateAccessorWithIndex<T>(asset, asset.accessors[it->second],
		[&](T value, std::size_t i)
		{
			vertices[vertexBase + i].*member = value;
		});
}

static void ExtractPrimitive(const fastgltf::Asset& asset,
							  const fastgltf::Primitive& prim,
							  Mesh& out)
{
	// Position is required — skip degenerate primitives.
	auto posIt = prim.findAttribute("POSITION");
	if (posIt == prim.attributes.end()) { return; }

	Submesh submesh;
	submesh.vertexOffset  = static_cast<u32>(out.vertices.size());
	submesh.indexOffset   = static_cast<u32>(out.indices.size());
	submesh.materialIndex = prim.materialIndex.has_value() ? static_cast<int32>(prim.materialIndex.value()) : -1;

	const auto& posAccessor = asset.accessors[posIt->second];
	const u32   vertexCount = static_cast<u32>(posAccessor.count);
	out.vertices.resize(out.vertices.size() + vertexCount);

	// Position
	fastgltf::iterateAccessorWithIndex<Vec3>(asset, posAccessor,
		[&](Vec3 v, std::size_t i)
		{
			out.vertices[submesh.vertexOffset + i].position = v;
		});

	// Normal
	{
		auto it = prim.findAttribute("NORMAL");
		if (it != prim.attributes.end())
		{
			fastgltf::iterateAccessorWithIndex<Vec3>(asset, asset.accessors[it->second],
				[&](Vec3 v, std::size_t i)
				{
					out.vertices[submesh.vertexOffset + i].normal = v;
				});
		}
	}

	// Tangent (vec4 — w is bitangent sign)
	{
		auto it = prim.findAttribute("TANGENT");
		if (it != prim.attributes.end())
		{
			fastgltf::iterateAccessorWithIndex<Vec4>(asset, asset.accessors[it->second],
				[&](Vec4 v, std::size_t i)
				{
					out.vertices[submesh.vertexOffset + i].tangent = v;
				});
		}
	}

	// UV0
	{
		auto it = prim.findAttribute("TEXCOORD_0");
		if (it != prim.attributes.end())
		{
			fastgltf::iterateAccessorWithIndex<Vec2>(asset, asset.accessors[it->second],
				[&](Vec2 v, std::size_t i)
				{
					out.vertices[submesh.vertexOffset + i].uv0 = v;
				});
		}
	}

	// UV1
	{
		auto it = prim.findAttribute("TEXCOORD_1");
		if (it != prim.attributes.end())
		{
			fastgltf::iterateAccessorWithIndex<Vec2>(asset, asset.accessors[it->second],
				[&](Vec2 v, std::size_t i)
				{
					out.vertices[submesh.vertexOffset + i].uv1 = v;
				});
		}
	}

	// Vertex colour (fastgltf normalises u8/u16 to float for us)
	{
		auto it = prim.findAttribute("COLOR_0");
		if (it != prim.attributes.end())
		{
			fastgltf::iterateAccessorWithIndex<Vec4>(asset, asset.accessors[it->second],
				[&](Vec4 v, std::size_t i)
				{
					out.vertices[submesh.vertexOffset + i].color = v;
				});
		}
	}

	// Indices — request u32; fastgltf promotes u8/u16 automatically.
	if (prim.indicesAccessor.has_value())
	{
		const auto& idxAccessor = asset.accessors[prim.indicesAccessor.value()];
		submesh.indexCount = static_cast<u32>(idxAccessor.count);
		out.indices.resize(out.indices.size() + idxAccessor.count);

		fastgltf::iterateAccessorWithIndex<u32>(asset, idxAccessor,
			[&](u32 index, std::size_t i)
			{
				out.indices[submesh.indexOffset + i] = index;
			});
	}

	// Per-submesh AABB
	{
		Vector<DirectX::XMFLOAT3> positions(vertexCount);
		for (u32 i = 0; i < vertexCount; ++i)
		{
			positions[i] = out.vertices[submesh.vertexOffset + i].position;
		}
		BoundingBox::CreateFromPoints(submesh.bounds, vertexCount,
									  positions.data(), sizeof(DirectX::XMFLOAT3));
	}

	out.submeshes.push_back(submesh);
}

// ---------------------------------------------------------------------------
// MeshLoader::Load
// ---------------------------------------------------------------------------

URef<Mesh> MeshLoader::Load(const String& path)
{
	namespace fs = std::filesystem;

	const fs::path fsPath(path);

	fastgltf::Parser parser;

	fastgltf::GltfDataBuffer dataBuffer;
	if (!dataBuffer.loadFromFile(fsPath))
	{
		LOG_ERROR("MeshLoader: failed to read file: {}", path);
		return nullptr;
	}

	constexpr auto options =
		fastgltf::Options::DecomposeNodeMatrices |
		fastgltf::Options::LoadExternalBuffers;

	auto result = parser.loadGltf(&dataBuffer, fsPath.parent_path(), options);
	if (result.error() != fastgltf::Error::None)
	{
		LOG_ERROR("MeshLoader: failed to parse glTF: {}", fastgltf::getErrorMessage(result.error()));
		return nullptr;
	}

	fastgltf::Asset& asset = result.get();

	auto mesh = std::make_unique<Mesh>();
	mesh->name = fsPath.stem().string();

	ExtractMaterials(asset, *mesh);
	ExtractTexturePaths(asset, *mesh);

	for (const auto& gltfMesh : asset.meshes)
	{
		for (const auto& prim : gltfMesh.primitives)
		{
			ExtractPrimitive(asset, prim, *mesh);
		}
	}

	// Whole-mesh AABB from all submesh AABBs
	if (!mesh->submeshes.empty())
	{
		mesh->bounds = mesh->submeshes[0].bounds;
		for (std::size_t i = 1; i < mesh->submeshes.size(); ++i)
		{
			BoundingBox::CreateMerged(mesh->bounds, mesh->bounds,
									  mesh->submeshes[i].bounds);
		}
	}

	LOG_DEBUG("MeshLoader: loaded '{}' — {} verts, {} indices, {} submeshes",
			  mesh->name, mesh->vertices.size(), mesh->indices.size(), mesh->submeshes.size());

	return mesh;
}

std::future<URef<Mesh>> MeshLoader::LoadAsync(const String& path, ThreadPool& pool)
{
	return pool.enqueue(Load, path);
}

Vector<std::future<URef<Mesh>>> MeshLoader::LoadBatch(const Vector<String>& paths, ThreadPool& pool)
{
	Vector<std::future<URef<Mesh>>> futures;
	futures.reserve(paths.size());

	for (const auto& path : paths)
	{
		futures.push_back(pool.enqueue(Load, path));
	}

	return futures;
}
