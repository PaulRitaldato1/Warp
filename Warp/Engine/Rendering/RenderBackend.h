#pragma once

#include <Common/CommonTypes.h>
#include <Rendering/Window/Window.h>
#include <Rendering/Renderer/Renderer.h>

class ImGuiBackend;

enum class GraphicsAPI : u8
{
	D3D12,
	Vulkan,
};

// Builds everything that depends on the graphics API: the device, through the
// renderer, and the ImGui renderer backend. The window is the same for both.
class RenderBackend
{
public:
	explicit RenderBackend(GraphicsAPI api);

	// r.GraphicsAPI from Config/Engine.ini, D3D12 or Vulkan. Without it, D3D12
	// on Windows and Vulkan elsewhere.
	static GraphicsAPI GetStartupAPI();

	static bool IsSupported(GraphicsAPI api);
	static const char* GetName(GraphicsAPI api);

	GraphicsAPI GetAPI() const
	{
		return m_api;
	}

	URef<IWindow> MakeWindow(const String& name, int width, int height) const;
	URef<Renderer> CreateRenderer(IWindow* window) const;
	URef<ImGuiBackend> CreateImGuiBackend() const;

private:
	GraphicsAPI m_api;
};
