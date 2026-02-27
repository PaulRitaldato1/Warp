#pragma once

#include <Common/CommonTypes.h>
#include <Memory/Arena.h>
#include <Threading/ThreadPool.h>
#include <Rendering/Renderer/CommandList.h>
#include <Rendering/Renderer/CommandQueue.h>
#include <Rendering/Renderer/Device.h>
#include <Rendering/Renderer/SwapChain.h>
#include <Rendering/Renderer/UploadBuffer.h>

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
	u64 computeFenceValue = 0;
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

	static constexpr u32 k_framesInFlight  = 3;
	static constexpr u64 k_uploadHeapSize  = 32 * 1024 * 1024; // 32 MB
	static constexpr u64 k_frameArenaSize  =  4 * 1024 * 1024; //  4 MB

	URef<Device>        m_device;
	URef<CommandQueue>  m_graphicsQueue;
	URef<CommandQueue>  m_computeQueue;
	URef<CommandQueue>  m_copyQueue;
	URef<SwapChain>     m_swapChain;

	RenderPath          m_renderPath = RenderPath::Deferred;

	Array<FrameSyncPoint, k_framesInFlight> m_frameSyncPoints;
	u32                                     m_frameIndex = 0;

	// Per-worker command lists — long-lived, reset each frame via Begin(frameIndex).
	// Indexed by worker thread index.
	Vector<URef<CommandList>> m_graphicsLists;
	Vector<URef<CommandList>> m_computeLists;
	URef<CommandList>         m_copyList;

	// CPU-side bump allocator: Reset() at the start of every frame.
	// Use for transient per-frame structures (draw lists, sort keys, etc.).
	Arena             m_frameArena{k_frameArenaSize};

	// GPU-visible ring buffer: OnBeginFrame() retires the oldest frame's slice.
	// Use for constant buffer data (transforms, material params, light data).
	URef<UploadBuffer> m_uploadBuffer;

	URef<ThreadPool>  m_workerPool;
};
