#pragma once

#include <Common/CommonTypes.h>
#include <Rendering/Resource/MeshResource.h>
#include <Rendering/Resource/TextureResource.h>
#include <Rendering/Renderer/Buffer.h>
#include <Rendering/Renderer/TextureUpload.h>

#include <future>

class Device;
class ThreadPool;

class WARP_API ResourceManager
{
public:
	ResourceManager()                                    = default;
	ResourceManager(const ResourceManager&)              = delete;
	ResourceManager& operator=(const ResourceManager&)   = delete;

	void Initialize(Device* device, ThreadPool* threadPool);
	void Shutdown();

	// Called by Renderer each frame to advance upload state machines.
	// Checks async load futures, creates GPU buffers, calls UploadData().
	void ProcessPendingUploads();

	// Drains completed staging uploads for the Renderer to queue.
	// Renderer calls this in BeginFrame and passes each to QueueStagingUpload().
	Vector<PendingStagingUpload> DrainStagingUploads();

	// Drains completed texture uploads for the Renderer to queue.
	// Renderer calls this in BeginFrame and passes each to QueueTextureUpload().
	Vector<PendingTextureUpload> DrainTextureUploads();

	// Drains textures that just became Ready and need a CopyDest → ShaderResource barrier.
	// Renderer issues TransitionTexture on the graphics command list in BeginFrame.
	Vector<Texture*> DrainTextureBarriers();

	// Returns a MeshResource if ready, nullptr if still loading/uploading.
	// Automatically kicks off loading on first request.
	MeshResource* GetMeshResource(const char* path);

	// O(1) handle-based lookup. Returns nullptr if the handle is invalid.
	// Use this in the hot render loop once a handle has been cached from GetMeshResource.
	MeshResource* GetMeshResourceByHandle(u32 handle);

	// Returns a TextureResource if ready, nullptr if still loading/uploading.
	// Automatically kicks off loading on first request.
	TextureResource* GetTextureResource(const char* path);

	// O(1) handle-based lookup. Returns nullptr if the handle is invalid.
	TextureResource* GetTextureResourceByHandle(u32 handle);

	u32 materialTextureSlots = 5;

private:
	void BeginMeshLoad(const String& path);
	u32  BeginTextureLoad(const String& path);

	// Transition Loading -> Uploading: create GPU buffers, call UploadData().
	void FinalizeMeshUpload(const String& path, MeshResource& resource);
	void FinalizeTextureUpload(const String& path, TextureResource& resource);

	Device*     m_device     = nullptr;
	ThreadPool* m_threadPool = nullptr;

	// How many frames to wait after uploading before marking Ready.
	// Must match Renderer::k_framesInFlight so deferred copies complete.
	static constexpr u32 k_uploadWaitFrames = 3;

	// Cache: path -> resource. Entries persist for the lifetime of the manager.
	HashMap<String, URef<MeshResource>>    m_meshCache;
	HashMap<String, URef<TextureResource>> m_textureCache;

	// Handle tables: index -> raw pointer for O(1) render-loop lookups.
	Vector<MeshResource*>    m_meshByHandle;
	Vector<TextureResource*> m_textureByHandle;

	// Pending async loads tracked via futures.
	HashMap<String, std::future<URef<Mesh>>>        m_pendingMeshLoads;
	HashMap<String, std::future<URef<TextureData>>> m_pendingTextureLoads;

	// Staging uploads ready for Renderer to pick up.
	Vector<PendingStagingUpload>  m_readyStagingUploads;
	Vector<PendingTextureUpload>  m_readyTextureUploads;
	Vector<Texture*>              m_pendingTextureBarriers;
};
