#pragma once

#include <Common/CommonTypes.h>
#include <Core/Console/ConsoleObject.h>
#include <Debugging/RenderDocCapture.h>
#include <Events/DelegateDefs.h>
#include <Memory/Arena.h>
#include <Threading/ThreadPool.h>

#include <Renderer/DrawList.h>
#include <Rendering/Renderer/Buffer.h>
#include <Rendering/Renderer/CommandList.h>
#include <Rendering/Renderer/CommandQueue.h>
#include <Rendering/Renderer/DescriptorHandle.h>
#include <Rendering/Renderer/Device.h>
#include <Rendering/Renderer/GPUScene.h>
#include <Rendering/Renderer/Pipeline.h>
#include <Rendering/Renderer/PipelineLibrary.h>
#include <Rendering/Renderer/RenderGraph.h>
#include <Rendering/Renderer/RenderScene.h>
#include <Rendering/Renderer/Shader.h>
#include <Rendering/Renderer/SwapChain.h>
#include <Rendering/Renderer/Texture.h>
#include <Rendering/Renderer/TextureUpload.h>
#include <Rendering/Renderer/UploadBuffer.h>

#include <Rendering/Passes/GBufferPass.h>
#include <Rendering/Passes/ImGuiPass.h>
#include <Rendering/Passes/LightingPass.h>
#include <Rendering/Passes/ShadowPass.h>
#include <Rendering/Passes/TonemapPass.h>

class IWindow;
class World;
class ResourceManager;
class RenderDocCapture;

#include <UI/ImGuiBackend.h>

enum class RenderPath : u8
{
	Deferred,
	ForwardPlus
};

// Tracks the GPU fence values for one in-flight frame slot.
// When the render thread retires a slot it waits until both
// values are reached before reusing allocators / ring buffer memory.
struct FrameSyncPoint
{
	u64 graphicsFenceValue = 0;
	u64 computeFenceValue  = 0;
	u64 copyFenceValue	   = 0;
};

class Renderer
{
public:
	// How many frames the CPU is allowed to run ahead of the GPU. Controls the
	// number of command allocator slots, upload buffer slices and FrameSyncPoints.
	// Two gives CPU and GPU overlap without the extra latency of a third frame.
	// Public so the device can size its per-frame descriptor regions to match.
	static constexpr u32 k_framesInFlight = 2;

	// Matches k_framesInFlight, so every frame in flight has an image to acquire
	// and the CPU never stalls waiting for one.
	static constexpr u32 k_backBufferCount = k_framesInFlight;

	~Renderer();

	// window is non-owning — WarpEngine retains ownership.
	// device is moved in — Renderer takes ownership.
	void Init(IWindow* window, URef<Device> device);
	void Shutdown();
	void OnResize(u32 width, u32 height);

	// Blocks until every queue has finished all submitted work.
	void WaitForGPUIdle();

	// Frame loop — implemented once in Renderer.cpp using abstract types.
	// These are not virtual: render path logic is shared across all APIs.
	void BeginFrame();
	void Draw();
	void EndFrame();

	// The backend comes from RenderBackend, since it has to match the device's API.
	void InitImGui(IWindow* window, URef<ImGuiBackend> backend);
	void ShutdownImGui();
	void NewFrameImGui();
	bool IsImGuiInitialized() const;

	void SetRenderPath(RenderPath path)
	{
		m_renderPath = path;
	}

	RenderPath GetRenderPath() const
	{
		return m_renderPath;
	}

	void SetWorld(World* world);

	World* GetWorld() const
	{
		return m_world;
	}

	void SetResourceManager(ResourceManager* manager)
	{
		m_resourceManager = manager;
		m_gpuScene.SetResourceManager(manager);
	}

	ResourceManager* GetResourceManager() const
	{
		return m_resourceManager;
	}

	Device* GetDevice()
	{
		return m_device.get();
	}

	ThreadPool* GetThreadPool()
	{
		return m_workerPool.get();
	}

	// Last frame's per-entity frustum culling counts, for the debug UI.
	struct CullStats
	{
		u32 tested = 0;
		u32 culled = 0;
	};

	struct DrawStats
	{
		u32 batches	  = 0;
		u32 drawCalls = 0;

		// Read back from the draw args k_framesInFlight frames late, since with GPU
		// culling only the GPU knows what survived. Counted per submesh instance.
		u32 numTris			 = 0;
		u32 visibleInstances = 0;

		// Changes since last frame, and the slots re-uploaded because of them.
		u32 dirtyTransforms = 0;
		u32 dirtyMeshes		= 0;
		u32 removedMeshes	= 0;
		u32 uploadedSlots	= 0;
	};

	// Both last frame's, built from the GPU scene's stats.
	CullStats GetCullStats() const;
	DrawStats GetDrawStats() const;

	const RenderGraph& GetRenderGraph() const
	{
		return m_renderGraph;
	}

	// The r.GPUCulling cvar. The CPU cull is kept as a reference to compare against.
	void SetGPUCulling(bool bEnabled);
	bool IsGPUCulling() const;

protected:
	// Render path implementations — filled out as the engine matures.
	// Both live in Renderer.cpp and use only abstract CommandList/Queue types.
	void DrawDeferred();
	void DrawForwardPlus();

	// Every pass, for the calls that do not care about order: Init and OnResize.
	Array<RenderPass*, 5> GetPasses();

	// Split k_framesInFlight ways. Instance data only passes through here when it
	// changes, but a full upload after a grow is 16 MB at 100k instances.
	static constexpr u64 k_uploadHeapSize = 128 * 1024 * 1024; // 128 MB
	static constexpr u64 k_frameArenaSize = 4 * 1024 * 1024;   //  4 MB

	URef<Device> m_device;
	URef<CommandQueue> m_graphicsQueue;
	URef<CommandQueue> m_computeQueue;
	URef<CommandQueue> m_copyQueue;
	URef<SwapChain> m_swapChain;

	// Declared after m_device, so it is destroyed first. Handles stay valid across
	// r.ReloadShaders, so passes look the PSO up each time they bind it.
	URef<PipelineLibrary> m_pipelines;

	// These own pipelines, buffers and textures, so they are destroyed before the
	// device too. DrawDeferred decides their order.
	GPUScene m_gpuScene;
	ShadowPass m_shadowPass;
	GBufferPass m_gbufferPass;
	LightingPass m_lightingPass;
	TonemapPass m_tonemapPass;
	ImGuiPass m_imguiPass;

	// Set by r.ReloadShaders, which runs mid frame inside the ImGui build. The
	// reload waits for the next BeginFrame, before anything records.
	void OnReloadShaders(const ConsoleArgs& args);
	URef<MemberFuncType<Renderer, const ConsoleArgs&>> m_reloadShadersDelegate;
	bool m_bReloadShadersRequested = false;

	// Filled and executed by DrawDeferred every frame. A member only so the pass
	// list keeps its capacity and the window can read last frame's passes.
	RenderGraph m_renderGraph;

	World* m_world					   = nullptr; // non-owning — set by WarpEngine
	ResourceManager* m_resourceManager = nullptr; // non-owning — set by WarpEngine
	RenderPath m_renderPath			   = RenderPath::Deferred;

	Array<FrameSyncPoint, k_framesInFlight> m_frameSyncPoints;
	u32 m_frameIndex = 0;

	// Recorded by last frame's graph, for GetDrawStats.
	u32 m_drawCalls = 0;

	// Per-worker command lists — long-lived, reset each frame via Begin(frameIndex).
	// Indexed by worker thread index.
	Vector<URef<CommandList>> m_graphicsLists;
	Vector<URef<CommandList>> m_computeLists;
	URef<CommandList> m_copyList;
	URef<CommandList> m_urgentCopyList; // Separate list for same-frame copies.

	// CPU-side bump allocator: Reset() at the start of every frame.
	// Use for transient per-frame structures (draw lists, sort keys, etc.).
	Arena m_frameArena{ k_frameArenaSize };

	// GPU-visible ring buffer: OnBeginFrame() retires the oldest frame's slice.
	// Use for constant buffer data (transforms, material params, light data).
	URef<UploadBuffer> m_uploadBuffer;

	URef<ThreadPool> m_workerPool;

	// ---------------------------------------------------------------------------
	// Staging upload lifecycle
	// ---------------------------------------------------------------------------

	// Deferred uploads — copied on the copy queue, available within k_framesInFlight frames.
	Vector<PendingStagingUpload> m_deferredUploads;

	// Urgent uploads — copied and fenced before graphics/compute submission this frame.
	Vector<PendingStagingUpload> m_urgentUploads;

	// Staging buffers that have been submitted for copy, paired with the
	// copy queue fence value. Freed once the GPU completes past that value.
	struct InFlightStaging
	{
		URef<Buffer> stagingBuffer;
		u64 fenceValue;
	};
	Vector<InFlightStaging> m_inFlightStagingBuffers;

	// Texture uploads queued this frame — recorded onto m_copyList in EndFrame.
	Vector<PendingTextureUpload> m_deferredTextureUploads;

	// Texture staging buffers in-flight on the GPU copy queue.
	// Freed once the copy queue fence value is reached.
	struct InFlightTextureUpload
	{
		URef<UploadBuffer> stagingBuffer;
		u64 fenceValue;
	};
	Vector<InFlightTextureUpload> m_inFlightTextureUploads;

	// Cross-queue wait flags — set by QueueCopyForThisFrame, consumed by EndFrame.
	bool m_graphicsWaitOnCopy = false;
	bool m_computeWaitOnCopy  = false;

	URef<RenderDocCapture> m_renderDoc = std::make_unique<RenderDocCapture>();

	bool m_imguiInitialized = false;
	URef<ImGuiBackend> m_imguiBackend;

public:
	// Queue a staging upload for the Renderer to process.
	// Data is available within k_framesInFlight frames.
	void QueueStagingUpload(PendingStagingUpload& upload);

	// Queue a staging upload that must be available THIS frame.
	// Inserts a cross-queue GPU wait on the specified queue so it won't
	// execute until the copy completes.
	void QueueCopyForThisFrame(PendingStagingUpload& upload, CommandQueueType queueType);

	// Queue a texture upload for this frame's copy list.
	// All mips are recorded onto m_copyList in EndFrame.
	void QueueTextureUpload(PendingTextureUpload& upload);
};
