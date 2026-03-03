#include <Rendering/Resource/ResourceManager.h>
#include <Rendering/Mesh/MeshLoader.h>
#include <Rendering/Mesh/Mesh.h>
#include <Rendering/Texture/TextureLoader.h>
#include <Rendering/Texture/TextureData.h>
#include <Rendering/Renderer/Device.h>
#include <Rendering/Renderer/Texture.h>
#include <Threading/ThreadPool.h>
#include <Debugging/Logging.h>
#include <Debugging/Assert.h>

void ResourceManager::Initialize(Device* device, ThreadPool* threadPool)
{
	FATAL_ASSERT(device, "ResourceManager::Initialize: device is null");
	FATAL_ASSERT(threadPool, "ResourceManager::Initialize: threadPool is null");

	m_device     = device;
	m_threadPool = threadPool;

	LOG_DEBUG("ResourceManager initialized");
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
				resource.mesh = std::move(mesh);
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
				resource.textureData = std::move(textureData);
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

void ResourceManager::BeginMeshLoad(const String& path)
{
	URef<MeshResource> resource = std::make_unique<MeshResource>();
	resource->state = AssetState::Loading;
	m_meshCache[path] = std::move(resource);

	m_pendingMeshLoads[path] = MeshLoader::LoadAsync(path, *m_threadPool);
}

void ResourceManager::BeginTextureLoad(const String& path)
{
	URef<TextureResource> resource = std::make_unique<TextureResource>();
	resource->state = AssetState::Loading;
	m_textureCache[path] = std::move(resource);

	m_pendingTextureLoads[path] = TextureLoader::LoadAsync(path, *m_threadPool);
}

void ResourceManager::FinalizeMeshUpload(const String& path, MeshResource& resource)
{
	const Mesh& mesh = *resource.mesh;

	// Create vertex buffer.
	BufferDesc vertexDesc;
	vertexDesc.type        = BufferType::Vertex;
	vertexDesc.numElements = static_cast<u32>(mesh.vertices.size());
	vertexDesc.stride      = static_cast<u32>(sizeof(Vertex));
	vertexDesc.name        = path + "_VB";
	resource.vertexBuffer  = m_device->CreateBuffer(vertexDesc);

	// Create index buffer.
	BufferDesc indexDesc;
	indexDesc.type        = BufferType::Index;
	indexDesc.numElements = static_cast<u32>(mesh.indices.size());
	indexDesc.stride      = static_cast<u32>(sizeof(u32));
	indexDesc.name        = path + "_IB";
	resource.indexBuffer  = m_device->CreateBuffer(indexDesc);

	// Upload vertex data.
	PendingStagingUpload vertexUpload = resource.vertexBuffer->UploadData(
		mesh.vertices.data(),
		mesh.vertices.size() * sizeof(Vertex));
	if (vertexUpload.IsValid())
	{
		m_readyStagingUploads.push_back(std::move(vertexUpload));
	}

	// Upload index data.
	PendingStagingUpload indexUpload = resource.indexBuffer->UploadData(
		mesh.indices.data(),
		mesh.indices.size() * sizeof(u32));
	if (indexUpload.IsValid())
	{
		m_readyStagingUploads.push_back(std::move(indexUpload));
	}

	resource.state = AssetState::Uploading;
	resource.uploadFrameCounter = 0;
}

void ResourceManager::FinalizeTextureUpload(const String& path, TextureResource& resource)
{
	const TextureData& texData = *resource.textureData;

	// Create GPU texture.
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

	// TODO: Upload texture data mip-by-mip via staging buffers.
	// Texture upload requires per-mip CopyTextureRegion / vkCmdCopyBufferToImage,
	// which is more involved than buffer copies. Leaving as a stub for now —
	// the mesh pipeline is the priority.

	resource.state = AssetState::Uploading;
	resource.uploadFrameCounter = 0;
}
