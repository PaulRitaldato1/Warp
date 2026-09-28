#pragma once

#include <Common/CommonTypes.h>

class IWindow;
class Device;
class CommandList;
class CommandQueue;

// Renderer-side ImGui backend, one per graphics API. Both share GLFW as the
// platform backend, so input handling is identical under either API.
// Created by RenderBackend::CreateImGuiBackend.
class ImGuiBackend
{
public:
	virtual ~ImGuiBackend() = default;

	virtual bool Init(IWindow* window, Device* device, CommandQueue* graphicsQueue, u32 framesInFlight) = 0;
	virtual void Shutdown() = 0;
	virtual void NewFrame() = 0;
	virtual void Render(CommandList* commandList) = 0;

protected:
	// The API-independent half: context, style, and a font sized for the
	// window's DPI so text is sharp from the first frame.
	static void CreateContext(IWindow* window);
};
