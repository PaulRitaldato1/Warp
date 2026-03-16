#ifdef WARP_WINDOWS

#include <Rendering/RenderBackend.h>
#include <Rendering/Renderer/Platform/Windows/WindowsWindow.h>
#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12Device.h>
#include <Debugging/Logging.h>

class WindowsRenderBackend : public RenderBackend
{
public:
	URef<IWindow> MakeWindow(const String& name, int width, int height) override
	{
		return std::make_unique<WindowsWindow>(name, width, height);
	}

	URef<Renderer> CreateRenderer(IWindow* window) override
	{
		// Renderer constructor loads RenderDoc — must happen before device
		// creation so RenderDoc can hook the graphics API calls.
		URef<Renderer> renderer = std::make_unique<Renderer>();

		DeviceDesc deviceDesc;
#if defined(WARP_DEBUG)
		deviceDesc.bEnableDebugLayer = true;
#endif
		URef<D3D12Device> device = std::make_unique<D3D12Device>();
		device->Initialize(deviceDesc);

		renderer->Init(window, std::move(device));
		return renderer;
	}
};

URef<RenderBackend> RenderBackend::Create()
{
	return std::make_unique<WindowsRenderBackend>();
}

#endif
