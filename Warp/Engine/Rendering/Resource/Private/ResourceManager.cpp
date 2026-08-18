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
#include <Common/PathRegistry.h>
#include <cstring>
#include <thread>

void ResourceManager::Initialize(Device* device, ThreadPool* threadPool)
{
	FATAL_ASSERT(device, "ResourceManager::Initialize: device is null");
	FATAL_ASSERT(threadPool, "ResourceManager::Initialize: threadPool is null");

	m_device = device;

	m_poolExecutor = std::make_unique<ThreadPoolExecutor>(*threadPool);

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

	const u32 pathId = GetPathRegistry().Intern("builtin://checkerboard");
	FinalizeTextureUpload(pathId, *resource);

	m_defaultTextureHandle = resource->handle;
	TextureResource* rawPtr = resource.get();
	m_textureByHandle.push_back(rawPtr);
	m_textureCache[pathId] = std::move(resource);

	MarkTextureReadyTask(pathId);
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

	const u32 pathId = GetPathRegistry().Intern("builtin://default_material");
	FinalizeTextureUpload(pathId, *resource);

	m_defaultMaterialTextureHandle = resource->handle;
	TextureResource* rawPtr = resource.get();
	m_textureByHandle.push_back(rawPtr);
	m_textureCache[pathId] = std::move(resource);

	MarkTextureReadyTask(pathId);
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

	const u32 pathId = GetPathRegistry().Intern("builtin://default_normal");
	FinalizeTextureUpload(pathId, *resource);

	m_defaultNormalTextureHandle = resource->handle;
	TextureResource* rawPtr = resource.get();
	m_textureByHandle.push_back(rawPtr);
	m_textureCache[pathId] = std::move(resource);

	MarkTextureReadyTask(pathId);
}

void ResourceManager::Shutdown()
{
	m_shuttingDown.store(true, std::memory_order_release);

	// In-flight tasks unwind at their next resume point. Pool-side steps finish on
	// their own; GPU-side and frame-wait continuations only run when pumped here.
	while (m_inFlightLoads.load(std::memory_order_acquire) > 0)
	{
		m_gpuExecutor.Drain();
		m_uploadFence.ReleaseAll();
		std::this_thread::yield();
	}

	m_meshCache.clear();
	m_textureCache.clear();
	m_readyStagingUploads.clear();

	LOG_DEBUG("ResourceManager shut down");
}

void ResourceManager::ProcessPendingUploads(u64 completedCopyValue)
{
	// Everything that was a polling loop now lives inside the load coroutines.
	// This just gives them somewhere to resume.
	m_gpuExecutor.Drain();
	m_uploadFence.Poll(completedCopyValue);
}

void ResourceManager::OnCopySubmitted(u64 fenceValue)
{
	m_uploadFence.OnSubmitted(fenceValue);
}

// ---------------------------------------------------------------------------
// Load tasks
// ---------------------------------------------------------------------------

DetachedTask ResourceManager::LoadMeshTask(u32 pathId)
{
	m_inFlightLoads.fetch_add(1, std::memory_order_acq_rel);

	// Copied rather than referenced: coroutine locals live in the frame, and a
	// value needs no reasoning about registry lifetime across suspension.
	const String path = GetPathRegistry().Resolve(pathId);

	co_await ResumeOn{ *m_poolExecutor };
	MeshLoadResult result = MeshLoader::Load(path);

	// Device calls are not thread safe, so everything below runs on the owning thread.
	co_await ResumeOn{ m_gpuExecutor };

	if (!m_shuttingDown.load(std::memory_order_acquire))
	{
		if (result)
		{
			MeshResource& resource = *m_meshCache[pathId];
			resource.mesh		   = std::move(*result);

			if (FinalizeMeshUpload(pathId, resource))
			{
				LOG_DEBUG("Mesh loaded and upload queued: {}", path);

				co_await UploadComplete{ m_uploadFence };

				if (!m_shuttingDown.load(std::memory_order_acquire))
				{
					resource.state = AssetState::Ready;
					LOG_DEBUG("Mesh ready: {}", path);
				}
			}
		}
		else
		{
			const LoadError& error = result.error();
			LOG_ERROR("Failed to load mesh '{}': [{}] {}", path, ToString(error.code), error.message);
		}
	}

	m_inFlightLoads.fetch_sub(1, std::memory_order_acq_rel);
}

DetachedTask ResourceManager::LoadTextureTask(u32 pathId)
{
	m_inFlightLoads.fetch_add(1, std::memory_order_acq_rel);

	const String path = GetPathRegistry().Resolve(pathId);

	co_await ResumeOn{ *m_poolExecutor };
	TextureLoadResult result = TextureLoader::Load(path);

	co_await ResumeOn{ m_gpuExecutor };

	if (!m_shuttingDown.load(std::memory_order_acquire))
	{
		if (result)
		{
			TextureResource& resource = *m_textureCache[pathId];
			resource.textureData	  = std::move(*result);

			if (FinalizeTextureUpload(pathId, resource))
			{
				LOG_DEBUG("Texture loaded and upload queued: {}", path);

				co_await UploadComplete{ m_uploadFence };

				if (!m_shuttingDown.load(std::memory_order_acquire))
				{
					resource.state = AssetState::Ready;
					m_pendingTextureBarriers.push_back(resource.gpuTexture.get());
					LOG_DEBUG("Texture ready: {}", path);
				}
			}
		}
		else
		{
			const LoadError& error = result.error();
			LOG_ERROR("Failed to load texture '{}': [{}] {}", path, ToString(error.code), error.message);
		}
	}

	m_inFlightLoads.fetch_sub(1, std::memory_order_acq_rel);
}

DetachedTask ResourceManager::MarkMeshReadyTask(u32 pathId)
{
	m_inFlightLoads.fetch_add(1, std::memory_order_acq_rel);

	co_await UploadComplete{ m_uploadFence };

	if (!m_shuttingDown.load(std::memory_order_acquire))
	{
		m_meshCache[pathId]->state = AssetState::Ready;
		LOG_DEBUG("Mesh ready: {}", GetPathRegistry().Resolve(pathId));
	}

	m_inFlightLoads.fetch_sub(1, std::memory_order_acq_rel);
}

DetachedTask ResourceManager::MarkTextureReadyTask(u32 pathId)
{
	m_inFlightLoads.fetch_add(1, std::memory_order_acq_rel);

	co_await UploadComplete{ m_uploadFence };

	if (!m_shuttingDown.load(std::memory_order_acquire))
	{
		TextureResource& resource = *m_textureCache[pathId];
		resource.state			  = AssetState::Ready;
		m_pendingTextureBarriers.push_back(resource.gpuTexture.get());
		LOG_DEBUG("Texture ready: {}", GetPathRegistry().Resolve(pathId));
	}

	m_inFlightLoads.fetch_sub(1, std::memory_order_acq_rel);
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

u32 ResourceManager::RegisterMesh(u32 pathId, URef<Mesh> mesh)
{
	DYNAMIC_ASSERT(mesh, "ResourceManager::RegisterMesh: mesh is null");
	DYNAMIC_ASSERT(m_meshCache.find(pathId) == m_meshCache.end(),
	               "ResourceManager::RegisterMesh: mesh already registered");

	URef<MeshResource> resource = std::make_unique<MeshResource>();
	resource->mesh = std::move(mesh);

	u32 handle = static_cast<u32>(m_meshByHandle.size());
	resource->handle = handle;

	FinalizeMeshUpload(pathId, *resource);

	resource->state = AssetState::Uploading;

	MeshResource* rawPtr = resource.get();
	m_meshByHandle.push_back(rawPtr);
	m_meshCache[pathId] = std::move(resource);

	MarkMeshReadyTask(pathId);

	LOG_DEBUG("ResourceManager: registered mesh '{}'", GetPathRegistry().Resolve(pathId));
	return handle;
}

u32 ResourceManager::CreatePlane(f32 sizeX, f32 sizeZ, u32 segmentsX, u32 segmentsZ)
{
	const u32 pathId = GetPathRegistry().Intern("builtin://plane");
	auto it = m_meshCache.find(pathId);
	if (it != m_meshCache.end())
	{
		return it->second->handle;
	}
	return RegisterMesh(pathId, GeoGenerator::CreatePlane(sizeX, sizeZ, segmentsX, segmentsZ));
}

u32 ResourceManager::CreateBox(f32 sizeX, f32 sizeY, f32 sizeZ)
{
	const u32 pathId = GetPathRegistry().Intern("builtin://box");
	auto it = m_meshCache.find(pathId);
	if (it != m_meshCache.end())
	{
		return it->second->handle;
	}
	return RegisterMesh(pathId, GeoGenerator::CreateBox(sizeX, sizeY, sizeZ));
}

u32 ResourceManager::RequestMesh(u32 pathId)
{
	if (pathId == PathRegistry::k_invalidId)
	{
		return ~0u;
	}

	auto it = m_meshCache.find(pathId);
	if (it != m_meshCache.end())
	{
		return it->second->handle;
	}

	// The handle is assigned up front, so callers can cache it immediately and
	// simply see a null resource until the upload lands.
	BeginMeshLoad(pathId);
	return m_meshCache[pathId]->handle;
}

void ResourceManager::AssignMesh(MeshComponent& mesh, const char* path)
{
	mesh.SetPath(path);
	mesh.meshHandle = RequestMesh(mesh.pathId);
}

MeshResource* ResourceManager::GetMeshResource(u32 pathId)
{
	auto it = m_meshCache.find(pathId);
	if (it != m_meshCache.end())
	{
		if (it->second->state == AssetState::Ready)
		{
			return it->second.get();
		}
		return nullptr; // Still loading or uploading
	}

	// Not found — kick off async load.
	BeginMeshLoad(pathId);
	return nullptr;
}

TextureResource* ResourceManager::GetTextureResource(u32 pathId)
{
	auto it = m_textureCache.find(pathId);
	if (it != m_textureCache.end())
	{
		if (it->second->state == AssetState::Ready)
		{
			return it->second.get();
		}
		return nullptr;
	}

	BeginTextureLoad(pathId);
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

void ResourceManager::BeginMeshLoad(u32 pathId)
{
	URef<MeshResource> resource = std::make_unique<MeshResource>();
	resource->state				= AssetState::Loading;
	resource->handle			= static_cast<u32>(m_meshByHandle.size());

	m_meshByHandle.push_back(resource.get());
	m_meshCache[pathId] = std::move(resource);

	LoadMeshTask(pathId);
}

MeshResource* ResourceManager::GetMeshResourceByHandle(u32 handle)
{
	if (handle >= static_cast<u32>(m_meshByHandle.size()))
	{
		return nullptr;
	}
	MeshResource* resource = m_meshByHandle[handle];
	if (!resource || resource->state != AssetState::Ready)
	{
		return nullptr;
	}
	return resource;
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

u32 ResourceManager::BeginTextureLoad(u32 pathId)
{
	auto it = m_textureCache.find(pathId);
	if (it != m_textureCache.end())
	{
		return it->second->handle;
	}

	URef<TextureResource> resource = std::make_unique<TextureResource>();
	resource->state				   = AssetState::Loading;
	resource->handle			   = static_cast<u32>(m_textureByHandle.size());
	u32 handle					   = resource->handle;
	m_textureByHandle.push_back(resource.get());
	m_textureCache[pathId]		   = std::move(resource);

	LoadTextureTask(pathId);
	return handle;
}

bool ResourceManager::FinalizeMeshUpload(u32 pathId, MeshResource& resource)
{
	// Needed for GPU debug names and for resolving the mesh's relative texture paths.
	const String& path = GetPathRegistry().Resolve(pathId);

	const Mesh& mesh = *resource.mesh;
	bool        queuedAnything = false;

	// Create the position stream. Bound alone by depth-only passes.
	BufferDesc positionDesc;
	positionDesc.type		 = BufferType::Vertex;
	positionDesc.numElements = mesh.VertexCount();
	positionDesc.stride		 = static_cast<u32>(sizeof(Vec3));
	positionDesc.name		 = path + "_PositionVB";
	resource.positionBuffer	 = m_device->CreateBuffer(positionDesc);

	// Create the attribute stream.
	BufferDesc attributeDesc;
	attributeDesc.type		  = BufferType::Vertex;
	attributeDesc.numElements = mesh.VertexCount();
	attributeDesc.stride	  = static_cast<u32>(sizeof(VertexAttributes));
	attributeDesc.name		  = path + "_AttributeVB";
	resource.attributeBuffer  = m_device->CreateBuffer(attributeDesc);

	// Create index buffer.
	BufferDesc indexDesc;
	indexDesc.type		  = BufferType::Index;
	indexDesc.numElements = static_cast<u32>(mesh.indices.size());
	indexDesc.stride	  = static_cast<u32>(sizeof(u32));
	indexDesc.name		  = path + "_IB";
	resource.indexBuffer  = m_device->CreateBuffer(indexDesc);

	// Upload position data.
	PendingStagingUpload positionUpload =
		resource.positionBuffer->UploadData(mesh.positions.data(), mesh.positions.size() * sizeof(Vec3));
	if (positionUpload.IsValid())
	{
		m_readyStagingUploads.push_back(std::move(positionUpload));
		queuedAnything = true;
	}

	// Upload attribute data.
	PendingStagingUpload attributeUpload = resource.attributeBuffer->UploadData(
		mesh.attributes.data(), mesh.attributes.size() * sizeof(VertexAttributes));
	if (attributeUpload.IsValid())
	{
		m_readyStagingUploads.push_back(std::move(attributeUpload));
		queuedAnything = true;
	}

	// Upload index data.
	PendingStagingUpload indexUpload =
		resource.indexBuffer->UploadData(mesh.indices.data(), mesh.indices.size() * sizeof(u32));
	if (indexUpload.IsValid())
	{
		m_readyStagingUploads.push_back(std::move(indexUpload));
		queuedAnything = true;
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
		const String fullPath      = dir + mesh.texturePaths[i];
		resource.textureHandles[i] = BeginTextureLoad(GetPathRegistry().Intern(fullPath));
	}

	resource.state = AssetState::Uploading;
	return queuedAnything;
}

bool ResourceManager::FinalizeTextureUpload(u32 pathId, TextureResource& resource)
{
	const String& path = GetPathRegistry().Resolve(pathId);

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
		return false;
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

	resource.state = AssetState::Uploading;
	return true;
}
