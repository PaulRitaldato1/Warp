#pragma once

#include <Common/CommonTypes.h>

class IWindow;

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
	u32 BufferCount = 2; // Number of backbuffers
	SwapChainFormat Format;
	bool bUseVsync = false;
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

	virtual ~SwapChain() = default;

private:
};