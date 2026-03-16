#ifdef WARP_LINUX

#include <Rendering/RenderBackend.h>
#include <Rendering/Renderer/Platform/Linux/LinuxWindow.h>
#include <Rendering/Renderer/Platform/Vulkan/VKDevice.h>
#include <Debugging/Logging.h>

class LinuxRenderBackend : public RenderBackend
{
public:
	URef<IWindow> MakeWindow(const String& name, int width, int height) override
	{
		return std::make_unique<LinuxWindow>(name, width, height);
	}

	URef<Renderer> CreateRenderer(IWindow* window) override
	{
		URef<Renderer> renderer = std::make_unique<Renderer>();

		DeviceDesc deviceDesc;
#if defined(WARP_DEBUG)
		deviceDesc.bEnableDebugLayer = true;
#endif
		URef<VKDevice> device = std::make_unique<VKDevice>();
		device->Initialize(deviceDesc);

		renderer->Init(window, std::move(device));
		return renderer;
	}
};

URef<RenderBackend> RenderBackend::Create()
{
	return std::make_unique<LinuxRenderBackend>();
}

#endif
