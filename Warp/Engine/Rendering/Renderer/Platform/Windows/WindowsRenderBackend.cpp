#ifdef WARP_WINDOWS

#include <Rendering/RenderBackend.h>
#include <Rendering/Renderer/Platform/Windows/WindowsWindow.h>
#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12Renderer.h>

class WindowsRenderBackend : public RenderBackend
{
public:
	URef<IWindow> CreateWindow(const String& name, int width, int height) override
	{
		return std::make_unique<WindowsWindow>(name, width, height);
	}

	URef<Renderer> CreateRenderer() override
	{
		return std::make_unique<D3D12Renderer>();
	}
};

URef<RenderBackend> RenderBackend::Create()
{
	return std::make_unique<WindowsRenderBackend>();
}

#endif
