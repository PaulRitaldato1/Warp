#ifdef WARP_LINUX

#include <Rendering/RenderBackend.h>
#include <Rendering/Renderer/Platform/Linux/LinuxWindow.h>
#include <Debugging/Logging.h>

class LinuxRenderBackend : public RenderBackend
{
public:
	URef<IWindow> CreateWindow(const String& name, int width, int height) override
	{
		return std::make_unique<LinuxWindow>(name, width, height);
	}

	URef<Renderer> CreateRenderer() override
	{
		// TODO: Vulkan renderer
		LOG_WARNING("LinuxRenderBackend: no renderer backend available yet");
		return nullptr;
	}
};

URef<RenderBackend> RenderBackend::Create()
{
	return std::make_unique<LinuxRenderBackend>();
}

#endif
