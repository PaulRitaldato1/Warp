#pragma once

#include <Common/CommonTypes.h>
#include <Rendering/Renderer/DescriptorHandle.h>

class IWindow;
class CommandList;

enum class SwapChainFormat
{
	RGBA8, //
	BGRA8, // Windows default
	HDR10  // HDR
};

struct SwapChainDesc
{
	IWindow* Window;
	u32 Width;
	u32 Height;
	u32 BufferCount = 2;
	SwapChainFormat Format;
	bool bUseVsync = false;
	// D3D12: must point to the ID3D12CommandQueue* used for rendering.
	// DXGI binds the swap chain to this queue permanently — presents only
	// synchronise against it. Vulkan ignores this field (queue is per-present).
	void* nativeCommandQueue = nullptr;
};

class SwapChain
{
public:
	// Initialize the swap chain
	virtual void Initialize(const SwapChainDesc& Desc) = 0;

	// Present the current frame to the screen
	virtual void Present() = 0;

	// Resize the swap chain (e.g., when the window resizes)
	virtual void Resize(u32 Width, u32 Height) = 0;

	// Get the current back buffer (to render into)
	virtual void* GetCurrentBackBuffer() const = 0;

	// Get the format of the swap chain
	virtual SwapChainFormat GetFormat() const = 0;

	// Get the width of the swap chain
	virtual u32 GetWidth() const = 0;

	// Get the height of the swap chain
	virtual u32 GetHeight() const = 0;

	// Cleanup resources
	virtual void Cleanup() = 0;

	// Returns the CPU descriptor handle for the current back buffer's RTV.
	virtual DescriptorHandle GetCurrentRTV() const = 0;

	// Records a resource barrier transitioning the current back buffer
	// from its present state to render-target and vice-versa.
	// Both must be called within an open command list.
	virtual void TransitionToRenderTarget(CommandList& cmd) = 0;
	virtual void TransitionToPresent(CommandList& cmd)      = 0;

	virtual ~SwapChain() = default;

private:
};