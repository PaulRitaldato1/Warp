#pragma once

#include <Common/CommonTypes.h>
#include <Memory/Arena.h>
#include <Threading/ThreadPool.h>
#include <Rendering/Renderer/Buffer.h>
#include <Rendering/Renderer/CommandList.h>
#include <Rendering/Renderer/CommandQueue.h>
#include <Rendering/Renderer/Device.h>
#include <Rendering/Renderer/SwapChain.h>
#include <Rendering/Renderer/UploadBuffer.h>
#include <Rendering/Renderer/Texture.h>
#include <Rendering/Renderer/DescriptorHandle.h>
#include <Rendering/Renderer/Shader.h>
#include <Rendering/Renderer/Pipeline.h>

#include <thread>
#include <future>

class IWindow;

enum class RenderPath
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
	u64 copyFenceValue     = 0;
};

class Renderer
{
public:
	virtual ~Renderer() = default;

	// window is non-owning — WarpEngine retains ownership
	// Platform-specific — implemented per API (D3D12, Vulkan, Metal)
	virtual void Init(IWindow* window)           = 0;
	virtual void Shutdown()                      = 0;
	virtual void OnResize(u32 width, u32 height) = 0;

	// Frame loop — implemented once in Renderer.cpp using abstract types.
	// These are not virtual: render path logic is shared across all APIs.
	void BeginFrame();
	void Draw();
	void EndFrame();

	void SetRenderPath(RenderPath path) { m_renderPath = path; }
	RenderPath GetRenderPath() const    { return m_renderPath; }

protected:
	// Render path implementations — filled out as the engine matures.
	// Both live in Renderer.cpp and use only abstract CommandList/Queue types.
	void DrawDeferred();
	void DrawForwardPlus();

	// Creates the test triangle shaders + PSO from the shared HLSL source below.
	// Called from platform Init() after the device and swap chain are ready.
	// Uses only abstract Device / Shader / PipelineState types — no platform casts.
	void CreateTestTriangle();

	// How many frames the CPU is allowed to run ahead of the GPU.
	// Controls the number of command allocator slots, upload buffer slices,
	// and FrameSyncPoints — NOT the swap chain back buffer count.
	static constexpr u32 k_framesInFlight  = 3;

	// Swap chain back buffers — independent of k_framesInFlight.
	// 2 is standard (double-buffering). The GPU renders into one while the
	// monitor displays the other.
	static constexpr u32 k_backBufferCount = 2;

	static constexpr u64 k_uploadHeapSize  = 32 * 1024 * 1024; // 32 MB
	static constexpr u64 k_frameArenaSize  =  4 * 1024 * 1024; //  4 MB

	URef<Device>        m_device;
	URef<CommandQueue>  m_graphicsQueue;
	URef<CommandQueue>  m_computeQueue;
	URef<CommandQueue>  m_copyQueue;
	URef<SwapChain>     m_swapChain;

	// Depth buffer shared across all render paths.
	// Created by the platform Init(); handle stored here for use in DrawXxx().
	URef<Texture>    m_depthTexture;
	DescriptorHandle m_depthHandle;

	// Test triangle — filled by CreateTestTriangle(), drawn in DrawDeferred().
	// Replace with a proper scene submission system once the pipeline is proven.
	URef<Shader>        m_testTriVS;
	URef<Shader>        m_testTriPS;
	URef<PipelineState> m_testTriPSO;

	// Shared HLSL source for the test triangle.
	// SV_VertexID-driven — no vertex buffer needed.
	// Both D3D12 and Vulkan backends compile this via their respective
	// shader compiler (DXC / shaderc).
	static constexpr const char* k_triVSSrc = R"hlsl(
float4 VSMain(uint vid : SV_VertexID) : SV_Position
{
    float2 pos[3] = {
        float2( 0.0,  0.5),
        float2( 0.5, -0.5),
        float2(-0.5, -0.5)
    };
    return float4(pos[vid], 0.0, 1.0);
}
)hlsl";

	static constexpr const char* k_triPSSrc = R"hlsl(
float4 PSMain() : SV_Target
{
    return float4(1.0, 0.5, 0.0, 1.0);
}
)hlsl";

	RenderPath          m_renderPath = RenderPath::Deferred;

	Array<FrameSyncPoint, k_framesInFlight> m_frameSyncPoints;
	u32                                     m_frameIndex = 0;

	// Per-worker command lists — long-lived, reset each frame via Begin(frameIndex).
	// Indexed by worker thread index.
	Vector<URef<CommandList>> m_graphicsLists;
	Vector<URef<CommandList>> m_computeLists;
	URef<CommandList>         m_copyList;
	URef<CommandList>         m_urgentCopyList;  // Separate list for same-frame copies.

	// CPU-side bump allocator: Reset() at the start of every frame.
	// Use for transient per-frame structures (draw lists, sort keys, etc.).
	Arena             m_frameArena{k_frameArenaSize};

	// GPU-visible ring buffer: OnBeginFrame() retires the oldest frame's slice.
	// Use for constant buffer data (transforms, material params, light data).
	URef<UploadBuffer> m_uploadBuffer;

	URef<ThreadPool>  m_workerPool;

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
		u64          fenceValue;
	};
	Vector<InFlightStaging> m_inFlightStagingBuffers;

	// Cross-queue wait flags — set by QueueCopyForThisFrame, consumed by EndFrame.
	bool m_graphicsWaitOnCopy = false;
	bool m_computeWaitOnCopy  = false;

public:
	// Queue a staging upload for the Renderer to process.
	// Data is available within k_framesInFlight frames.
	void QueueStagingUpload(PendingStagingUpload& upload);

	// Queue a staging upload that must be available THIS frame.
	// Inserts a cross-queue GPU wait on the specified queue so it won't
	// execute until the copy completes.
	void QueueCopyForThisFrame(PendingStagingUpload& upload, CommandQueueType queueType);
};
