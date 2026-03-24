#pragma once

#include <Common/CommonTypes.h>

class IWindow;
class Device;
class CommandList;
class CommandQueue;

// Abstract interface for platform-specific ImGui rendering backends.
// Implemented per-API (D3D12, Vulkan, etc.) in the platform directories.
class ImGuiBackend
{
public:
	virtual ~ImGuiBackend() = default;

	virtual bool Init(IWindow* window, Device* device, CommandQueue* graphicsQueue, u32 framesInFlight) = 0;
	virtual void Shutdown() = 0;
	virtual void NewFrame() = 0;
	virtual void Render(CommandList* commandList) = 0;
};

// Factory — returns the appropriate backend for the current platform/API.
WARP_API URef<ImGuiBackend> CreateImGuiBackend();
