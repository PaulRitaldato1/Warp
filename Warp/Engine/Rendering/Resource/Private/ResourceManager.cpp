#include <Rendering/Resource/ResourceManager.h>
#include <Rendering/Mesh/MeshLoader.h>
#include <Rendering/Mesh/Mesh.h>
#include <Rendering/Mesh/GeoGenerator.h>
#include <Rendering/Texture/TextureLoader.h>
#include <Rendering/Texture/TextureData.h>
#include <Rendering/Renderer/Device.h>
#include <Rendering/Renderer/Texture.h>
#include <Rendering/Renderer/UploadBuffer.h>
#include <Threading/ThreadPool.h>
#include <Debugging/Logging.h>
#include <Debugging/Assert.h>
#include <cstring>

void ResourceManager::Initialize(Device* device, ThreadPool* threadPool)
{
	FATAL_ASSERT(device, "ResourceManager::Initialize: device is null");
	FATAL_ASSERT(threadPool, "ResourceManager::Initialize: threadPool is null");

	m_device	 = device;
	m_threadPool = threadPool;

	CreateDefaultTexture();
	CreateDefaultMaterialTexture();
	CreateDefaultNormalTexture();

	LOG_DEBUG("ResourceManager initialized");
}

void ResourceManager::CreateDefaultTexture()
{
	constexpr u32 size      = 64;
	constexpr u32 blockSize = 8;
	constexpr u32 bpp       = 4;

	URef<TextureData> texData = std::make_unique<TextureData>();
	texData->name      = "builtin://checkerboard";
	texData->width     = size;
	texData->height    = size;
	texData->mipLevels = 1;
	texData->format    = TextureFormat::RGBA8;
	texData->data.resize(size * size * bpp);

	for (u32 y = 0; y < size; ++y)
	{
		for (u32 x = 0; x < size; ++x)
		{
			bool white = ((x / blockSize) + (y / blockSize)) % 2 == 0;
			u8 value   = white ? 200 : 50;
			u32 offset = (y * size + x) * bpp;
			texData->data[offset + 0] = value;
			texData->data[offset + 1] = value;
			texData->data[offset + 2] = value;
			texData->data[offset + 3] = 255;
		}
	}

	MipData mip;
	mip.data       = texData->data.data();
	mip.rowPitch   = size * bpp;
	mip.slicePitch = size * size * bpp;
	mip.width      = size;
	mip.height     = size;
	texData->mips.push_back(mip);

	URef<TextureResource> resource = std::make_unique<TextureResource>();
	resource->textureData = std::move(texData);
	resource->handle      = static_cast<u32>(m_textureByHandle.size());

	FinalizeTextureUpload("builtin://checkerboard", *resource);

	m_defaultTextureHandle = resource->handle;
	TextureResource* rawPtr = resource.get();
	m_textureByHandle.push_back(rawPtr);
	m_textureCache["builtin://checkerboard"] = std::move(resource);
}

void ResourceManager::CreateDefaultMaterialTexture()
{
	constexpr u32 size = 1;
	constexpr u32 bpp  = 4;

	URef<TextureData> texData = std::make_unique<TextureData>();
	texData->name      = "builtin://default_material";
	texData->width     = size;
	texData->height    = size;
	texData->mipLevels = 1;
	texData->format    = TextureFormat::RGBA8;
	texData->data.resize(bpp);

	// glTF metallic-roughness: R = occlusion, G = roughness, B = metallic.
	// Non-metallic, fully rough, full occlusion — a safe neutral default.
	texData->data[0] = 255; // R (occlusion = 1)
	texData->data[1] = 255; // G (roughness = 1)
	texData->data[2] = 0;   // B (metallic  = 0)
	texData->data[3] = 255; // A

	MipData mip;
	mip.data       = texData->data.data();
	mip.rowPitch   = size * bpp;
	mip.slicePitch = size * size * bpp;
	mip.width      = size;
	mip.height     = size;
	texData->mips.push_back(mip);

	URef<TextureResource> resource = std::make_unique<TextureResource>();
	resource->textureData = std::move(texData);
	resource->handle      = static_cast<u32>(m_textureByHandle.size());

	FinalizeTextureUpload("builtin://default_material", *resource);

	m_defaultMaterialTextureHandle = resource->handle;
	TextureResource* rawPtr = resource.get();
	m_textureByHandle.push_back(rawPtr);
	m_textureCache["builtin://default_material"] = std::move(resource);
}

void ResourceManager::CreateDefaultNormalTexture()
{
	constexpr u32 size = 1;
	constexpr u32 bpp  = 4;

	URef<TextureData> texData = std::make_unique<TextureData>();
	texData->name      = "builtin://default_normal";
	texData->width     = size;
	texData->height    = size;
	texData->mipLevels = 1;
	texData->format    = TextureFormat::RGBA8;
	texData->data.resize(bpp);

	// Tangent-space flat normal: (0, 0, 1) encoded as (128, 128, 255).
	texData->data[0] = 128; // R (X = 0)
	texData->data[1] = 128; // G (Y = 0)
	texData->data[2] = 255; // B (Z = 1)
	texData->data[3] = 255; // A

	MipData mip;
	mip.data       = texData->data.data();
	mip.rowPitch   = size * bpp;
	mip.slicePitch = size * size * bpp;
	mip.width      = size;
	mip.height     = size;
	texData->mips.push_back(mip);

	URef<TextureResource> resource = std::make_unique<TextureResource>();
	resource->textureData = std::move(texData);
	resource->handle      = static_cast<u32>(m_textureByHandle.size());

	FinalizeTextureUpload("builtin://default_normal", *resource);

	m_defaultNormalTextureHandle = resource->handle;
	TextureResource* rawPtr = resource.get();
	m_textureByHandle.push_back(rawPtr);
	m_textureCache["builtin://default_normal"] = std::move(resource);
}

void ResourceManager::Shutdown()
{
	// Wait for all pending loads to finish before clearing caches.
	for (auto& [path, future] : m_pendingMeshLoads)
	{
		if (future.valid())
		{
			future.wait();
		}
	}
	for (auto& [path, future] : m_pendingTextureLoads)
	{
		if (future.valid())
		{
			future.wait();
		}
	}

	m_pendingMeshLoads.clear();
	m_pendingTextureLoads.clear();
	m_meshCache.clear();
	m_textureCache.clear();
	m_readyStagingUploads.clear();

	LOG_DEBUG("ResourceManager shut down");
}

void ResourceManager::ProcessPendingUploads()
{
	// Check mesh load futures — transition Loading -> Uploading.
	Vector<String> completedMeshLoads;
	for (auto& [path, future] : m_pendingMeshLoads)
	{
		if (future.valid() && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		{
			URef<Mesh> mesh = future.get();
			if (mesh)
			{
				MeshResource& resource = *m_meshCache[path];
				resource.mesh		   = std::move(mesh);
				FinalizeMeshUpload(path, resource);
				LOG_DEBUG("Mesh loaded and upload queued: {}", path);
			}
			else
			{
				LOG_ERROR("Failed to load mesh: {}", path);
			}
			completedMeshLoads.push_back(path);
		}
	}

	for (const String& path : completedMeshLoads)
	{
		m_pendingMeshLoads.erase(path);
	}

	// Check texture load futures — transition Loading -> Uploading.
	Vector<String> completedTextureLoads;
	for (auto& [path, future] : m_pendingTextureLoads)
	{
		if (future.valid() && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		{
			URef<TextureData> textureData = future.get();
			if (textureData)
			{
				TextureResource& resource = *m_textureCache[path];
				resource.textureData	  = std::move(textureData);
				FinalizeTextureUpload(path, resource);
				LOG_DEBUG("Texture loaded and upload queued: {}", path);
			}
			else
			{
				LOG_ERROR("Failed to load texture: {}", path);
			}
			completedTextureLoads.push_back(path);
		}
	}

	for (const String& path : completedTextureLoads)
	{
		m_pendingTextureLoads.erase(path);
	}

	// Advance upload frame counters — transition Uploading -> Ready.
	for (auto& [path, resource] : m_meshCache)
	{
		if (resource->state == AssetState::Uploading)
		{
			resource->uploadFrameCounter++;
			if (resource->uploadFrameCounter >= k_uploadWaitFrames)
			{
				resource->state = AssetState::Ready;
				LOG_DEBUG("Mesh ready: {}", path);
			}
		}
	}

	for (auto& [path, resource] : m_textureCache)
	{
		if (resource->state == AssetState::Uploading)
		{
			resource->uploadFrameCounter++;
			if (resource->uploadFrameCounter >= k_uploadWaitFrames)
			{
				resource->state = AssetState::Ready;
				m_pendingTextureBarriers.push_back(resource->gpuTexture.get());
				LOG_DEBUG("Texture ready: {}", path);
			}
		}
	}
}

Vector<PendingStagingUpload> ResourceManager::DrainStagingUploads()
{
	Vector<PendingStagingUpload> uploads = std::move(m_readyStagingUploads);
	m_readyStagingUploads.clear();
	return uploads;
}

Vector<PendingTextureUpload> ResourceManager::DrainTextureUploads()
{
	Vector<PendingTextureUpload> uploads = std::move(m_readyTextureUploads);
	m_readyTextureUploads.clear();
	return uploads;
}

Vector<Texture*> ResourceManager::DrainTextureBarriers()
{
	Vector<Texture*> barriers = std::move(m_pendingTextureBarriers);
	m_pendingTextureBarriers.clear();
	return barriers;
}

u32 ResourceManager::RegisterMesh(const String& name, URef<Mesh> mesh)
{
	DYNAMIC_ASSERT(mesh, "ResourceManager::RegisterMesh: mesh is null");
	DYNAMIC_ASSERT(m_meshCache.find(name) == m_meshCache.end(),
	               "ResourceManager::RegisterMesh: mesh already registered");

	URef<MeshResource> resource = std::make_unique<MeshResource>();
	resource->mesh = std::move(mesh);

	u32 handle = static_cast<u32>(m_meshByHandle.size());
	resource->handle = handle;

	FinalizeMeshUpload(name, *resource);

	resource->state = AssetState::Uploading;
	resource->uploadFrameCounter = 0;

	MeshResource* rawPtr = resource.get();
	m_meshByHandle.push_back(rawPtr);
	m_meshCache[name] = std::move(resource);

	LOG_DEBUG("ResourceManager: registered mesh '{}'", name);
	return handle;
}

u32 ResourceManager::CreatePlane(f32 sizeX, f32 sizeZ, u32 segmentsX, u32 segmentsZ)
{
	auto it = m_meshCache.find("builtin://plane");
	if (it != m_meshCache.end())
	{
		return it->second->handle;
	}
	return RegisterMesh("builtin://plane", GeoGenerator::CreatePlane(sizeX, sizeZ, segmentsX, segmentsZ));
}

u32 ResourceManager::CreateBox(f32 sizeX, f32 sizeY, f32 sizeZ)
{
	auto it = m_meshCache.find("builtin://box");
	if (it != m_meshCache.end())
	{
		return it->second->handle;
	}
	return RegisterMesh("builtin://box", GeoGenerator::CreateBox(sizeX, sizeY, sizeZ));
}

MeshResource* ResourceManager::GetMeshResource(const char* path)
{
	String pathStr(path);

	auto it = m_meshCache.find(pathStr);
	if (it != m_meshCache.end())
	{
		if (it->second->state == AssetState::Ready)
		{
			return it->second.get();
		}
		return nullptr; // Still loading or uploading
	}

	// Not found — kick off async load.
	BeginMeshLoad(pathStr);
	return nullptr;
}

TextureResource* ResourceManager::GetTextureResource(const char* path)
{
	String pathStr(path);

	auto it = m_textureCache.find(pathStr);
	if (it != m_textureCache.end())
	{
		if (it->second->state == AssetState::Ready)
		{
			return it->second.get();
		}
		return nullptr;
	}

	BeginTextureLoad(pathStr);
	return nullptr;
}

Texture* ResourceManager::GetDefaultTexture()
{
	TextureResource* resource = GetTextureResourceByHandle(m_defaultTextureHandle);
	return resource ? resource->gpuTexture.get() : nullptr;
}

Texture* ResourceManager::GetDefaultMaterialTexture()
{
	TextureResource* resource = GetTextureResourceByHandle(m_defaultMaterialTextureHandle);
	return resource ? resource->gpuTexture.get() : nullptr;
}

Texture* ResourceManager::GetDefaultNormalTexture()
{
	TextureResource* resource = GetTextureResourceByHandle(m_defaultNormalTextureHandle);
	return resource ? resource->gpuTexture.get() : nullptr;
}

void ResourceManager::BeginMeshLoad(const String& path)
{
	URef<MeshResource> resource = std::make_unique<MeshResource>();
	resource->state				= AssetState::Loading;
	resource->handle			= static_cast<u32>(m_meshByHandle.size());

	m_meshByHandle.push_back(resource.get());
	m_meshCache[path] = std::move(resource);

	m_pendingMeshLoads[path] = MeshLoader::LoadAsync(path, *m_threadPool);
}

MeshResource* ResourceManager::GetMeshResourceByHandle(u32 handle)
{
	if (handle >= static_cast<u32>(m_meshByHandle.size()))
	{
		return nullptr;
	}
	return m_meshByHandle[handle];
}

TextureResource* ResourceManager::GetTextureResourceByHandle(u32 handle)
{
	if (handle >= static_cast<u32>(m_textureByHandle.size()))
	{
		return nullptr;
	}
	TextureResource* resource = m_textureByHandle[handle];
	if (!resource || resource->state != AssetState::Ready)
	{
		return nullptr;
	}
	return resource;
}

u32 ResourceManager::BeginTextureLoad(const String& path)
{
	auto it = m_textureCache.find(path);
	if (it != m_textureCache.end())
	{
		return it->second->handle;
	}

	URef<TextureResource> resource = std::make_unique<TextureResource>();
	resource->state				   = AssetState::Loading;
	resource->handle			   = static_cast<u32>(m_textureByHandle.size());
	u32 handle					   = resource->handle;
	m_textureByHandle.push_back(resource.get());
	m_textureCache[path]		   = std::move(resource);

	m_pendingTextureLoads[path] = TextureLoader::LoadAsync(path, *m_threadPool);
	return handle;
}

void ResourceManager::FinalizeMeshUpload(const String& path, MeshResource& resource)
{
	const Mesh& mesh = *resource.mesh;

	// Create vertex buffer.
	BufferDesc vertexDesc;
	vertexDesc.type		   = BufferType::Vertex;
	vertexDesc.numElements = static_cast<u32>(mesh.vertices.size());
	vertexDesc.stride	   = static_cast<u32>(sizeof(Vertex));
	vertexDesc.name		   = path + "_VB";
	resource.vertexBuffer  = m_device->CreateBuffer(vertexDesc);

	// Create index buffer.
	BufferDesc indexDesc;
	indexDesc.type		  = BufferType::Index;
	indexDesc.numElements = static_cast<u32>(mesh.indices.size());
	indexDesc.stride	  = static_cast<u32>(sizeof(u32));
	indexDesc.name		  = path + "_IB";
	resource.indexBuffer  = m_device->CreateBuffer(indexDesc);

	// Upload vertex data.
	PendingStagingUpload vertexUpload =
		resource.vertexBuffer->UploadData(mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex));
	if (vertexUpload.IsValid())
	{
		m_readyStagingUploads.push_back(std::move(vertexUpload));
	}

	// Upload index data.
	PendingStagingUpload indexUpload =
		resource.indexBuffer->UploadData(mesh.indices.data(), mesh.indices.size() * sizeof(u32));
	if (indexUpload.IsValid())
	{
		m_readyStagingUploads.push_back(std::move(indexUpload));
	}

	// Kick off texture loads for all textures referenced by this mesh.
	// Resolve relative paths against the mesh's directory.
	String dir(path);
	size_t lastSlash = dir.find_last_of("/\\");
	if (lastSlash != String::npos)
	{
		dir = dir.substr(0, lastSlash + 1);
	}
	else
	{
		dir.clear();
	}

	resource.textureHandles.resize(mesh.texturePaths.size(), ~0u);
	for (u32 i = 0; i < static_cast<u32>(mesh.texturePaths.size()); ++i)
	{
		String fullPath           = dir + mesh.texturePaths[i];
		resource.textureHandles[i] = BeginTextureLoad(fullPath);
	}

	resource.state				= AssetState::Uploading;
	resource.uploadFrameCounter = 0;
}

void ResourceManager::FinalizeTextureUpload(const String& path, TextureResource& resource)
{
	const TextureData& texData = *resource.textureData;

	// Create GPU texture in initial CopyDest state.
	TextureDesc textureDesc;
	textureDesc.type        = texData.type;
	textureDesc.width       = texData.width;
	textureDesc.height      = texData.height;
	textureDesc.depth       = texData.depth;
	textureDesc.mipLevels   = texData.mipLevels;
	textureDesc.arrayLayers = texData.arraySize;
	textureDesc.format      = texData.format;
	textureDesc.usage       = TextureUsage::Sampled;
	resource.gpuTexture     = m_device->CreateTexture(textureDesc);

	// D3D12 alignment requirements for texture staging data.
	static constexpr u32 k_rowPitchAlign   = 256; // D3D12_TEXTURE_DATA_PITCH_ALIGNMENT
	static constexpr u64 k_placementAlign  = 512; // D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT

	// Compute total staging size: each mip's rows are padded to k_rowPitchAlign,
	// and each mip's base offset is aligned to k_placementAlign.
	u64 totalBytes = 0;
	for (const MipData& mip : texData.mips)
	{
		const u32 srcRowPitch     = static_cast<u32>(mip.rowPitch);
		const u32 alignedRowPitch = (srcRowPitch + k_rowPitchAlign - 1) & ~(k_rowPitchAlign - 1);
		const u32 numRows         = (srcRowPitch > 0 && mip.slicePitch > 0)
		                              ? static_cast<u32>(mip.slicePitch / mip.rowPitch)
		                              : mip.height;
		totalBytes = (totalBytes + k_placementAlign - 1) & ~(k_placementAlign - 1);
		totalBytes += static_cast<u64>(alignedRowPitch) * numRows;
	}

	if (totalBytes == 0)
	{
		LOG_ERROR("ResourceManager: zero staging size for texture '{}'", path);
		return;
	}

	// Allocate the one-shot staging buffer.
	URef<UploadBuffer> stagingBuffer = m_device->CreateUploadBuffer(totalBytes, 1);

	PendingTextureUpload textureUpload;
	textureUpload.stagingUploadBuffer = std::move(stagingBuffer);
	textureUpload.destination         = resource.gpuTexture.get();

	for (u32 slice = 0; slice < texData.arraySize; ++slice)
	{
		for (u32 mipIdx = 0; mipIdx < texData.mipLevels; ++mipIdx)
		{
			const MipData& mip = texData.mips[slice * texData.mipLevels + mipIdx];

			const u32 srcRowPitch     = static_cast<u32>(mip.rowPitch);
			const u32 alignedRowPitch = (srcRowPitch + k_rowPitchAlign - 1) & ~(k_rowPitchAlign - 1);
			const u32 numRows         = (srcRowPitch > 0 && mip.slicePitch > 0)
			                              ? static_cast<u32>(mip.slicePitch / mip.rowPitch)
			                              : mip.height;
			const u64 mipStagingSize  = static_cast<u64>(alignedRowPitch) * numRows;

			UploadAllocation alloc = textureUpload.stagingUploadBuffer->Alloc(mipStagingSize, k_placementAlign);

			// Copy row-by-row, inserting padding between rows to meet alignment.
			u8*       dst = static_cast<u8*>(alloc.cpuPtr);
			const u8* src = mip.data;
			for (u32 row = 0; row < numRows; ++row)
			{
				memcpy(dst, src, srcRowPitch);
				dst += alignedRowPitch;
				src += srcRowPitch;
			}

			TextureMipUpload mipUpload;
			mipUpload.srcOffset   = alloc.offset;
			mipUpload.srcRowPitch = alignedRowPitch;
			mipUpload.mipLevel    = mipIdx;
			mipUpload.arraySlice  = slice;
			textureUpload.mips.push_back(mipUpload);
		}
	}

	m_readyTextureUploads.push_back(std::move(textureUpload));

	resource.state              = AssetState::Uploading;
	resource.uploadFrameCounter = 0;
}
