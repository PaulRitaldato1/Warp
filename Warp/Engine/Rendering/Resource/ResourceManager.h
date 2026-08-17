#pragma once

#include <Common/CommonTypes.h>
#include <Rendering/Mesh/MeshLoader.h>
#include <Rendering/Texture/TextureLoader.h>
#include <Rendering/Resource/MeshResource.h>
#include <Rendering/Resource/TextureResource.h>
#include <Rendering/Renderer/Buffer.h>
#include <Rendering/Renderer/TextureUpload.h>
#include <Threading/Executor.h>
#include <Threading/Task.h>

#include <atomic>

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

	// Called by Renderer each frame. Resumes load tasks waiting on the GPU thread,
	// and releases those whose upload copy has retired.
	void ProcessPendingUploads(u64 completedCopyValue);

	// Called by Renderer after submitting the frame's copy list, so uploads queued
	// this frame can be tied to the fence value that covers them.
	void OnCopySubmitted(u64 fenceValue);

	// Drains completed staging uploads for the Renderer to queue.
	// Renderer calls this in BeginFrame and passes each to QueueStagingUpload().
	Vector<PendingStagingUpload> DrainStagingUploads();

	// Drains completed texture uploads for the Renderer to queue.
	// Renderer calls this in BeginFrame and passes each to QueueTextureUpload().
	Vector<PendingTextureUpload> DrainTextureUploads();

	// Drains textures that just became Ready and need a CopyDest → ShaderResource barrier.
	// Renderer issues TransitionTexture on the graphics command list in BeginFrame.
	Vector<Texture*> DrainTextureBarriers();

	// Procedural geometry — creates GPU buffers and queues uploads. Returns the mesh handle.
	u32 CreatePlane(f32 sizeX = 10.f, f32 sizeZ = 10.f, u32 segmentsX = 1, u32 segmentsZ = 1);
	u32 CreateBox(f32 sizeX = 1.f, f32 sizeY = 1.f, f32 sizeZ = 1.f);

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

	// Returns the fallback checkerboard texture (used for BaseColor). Always valid after Initialize().
	Texture* GetDefaultTexture();

	// Returns a flat default texture for non-albedo material slots.
	// MetallicRoughness: (255, 255, 0) = full occlusion, fully rough, non-metallic.
	Texture* GetDefaultMaterialTexture();

	// Returns a flat normal map (128, 128, 255) = tangent-space (0, 0, 1).
	Texture* GetDefaultNormalTexture();


private:
	u32  RegisterMesh(const String& name, URef<Mesh> mesh);
	void CreateDefaultTexture();
	void CreateDefaultMaterialTexture();
	void CreateDefaultNormalTexture();
	void BeginMeshLoad(const String& path);
	u32  BeginTextureLoad(const String& path);

	// The whole load pipeline for one asset, start to Ready. Each co_await moves
	// execution to the thread that step needs.
	DetachedTask LoadMeshTask(String path);
	DetachedTask LoadTextureTask(String path);

	// Marks a resource Ready once its already-queued upload has had time to land.
	// Used by the procedural and builtin paths, which skip the load step.
	DetachedTask MarkMeshReadyTask(String path);
	DetachedTask MarkTextureReadyTask(String path);

	// Transition Loading -> Uploading: create GPU buffers, call UploadData().
	// Return false if nothing was queued, in which case there is no copy to wait on.
	bool FinalizeMeshUpload(const String& path, MeshResource& resource);
	bool FinalizeTextureUpload(const String& path, TextureResource& resource);

	Device*     m_device     = nullptr;
	ThreadPool* m_threadPool = nullptr;

	// Cache: path -> resource. Entries persist for the lifetime of the manager.
	HashMap<String, URef<MeshResource>>    m_meshCache;
	HashMap<String, URef<TextureResource>> m_textureCache;

	// Handle tables: index -> raw pointer for O(1) render-loop lookups.
	Vector<MeshResource*>    m_meshByHandle;
	Vector<TextureResource*> m_textureByHandle;

	// Fallback checkerboard texture used for BaseColor when no texture is assigned.
	u32 m_defaultTextureHandle = ~0u;

	// Flat default texture for non-albedo material slots (metallic-roughness, occlusion, emissive).
	u32 m_defaultMaterialTextureHandle = ~0u;

	// Flat normal map default: (128, 128, 255) = tangent-space (0, 0, 1).
	u32 m_defaultNormalTextureHandle = ~0u;

	// Coroutine scheduling. m_poolExecutor runs blocking file work, m_gpuExecutor
	// runs anything touching Device, m_uploadFence gates on the copy retiring.
	URef<ThreadPoolExecutor> m_poolExecutor;
	SerialExecutor           m_gpuExecutor;
	UploadFenceScheduler     m_uploadFence;

	// Load tasks are detached, so liveness is tracked separately for shutdown.
	std::atomic<u32>  m_inFlightLoads{ 0 };
	std::atomic<bool> m_shuttingDown{ false };

	// Staging uploads ready for Renderer to pick up.
	Vector<PendingStagingUpload>  m_readyStagingUploads;
	Vector<PendingTextureUpload>  m_readyTextureUploads;
	Vector<Texture*>              m_pendingTextureBarriers;
};
