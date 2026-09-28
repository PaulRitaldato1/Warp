#include <Rendering/RenderBackend.h>

#include <Core/Config/ConfigFile.h>
#include <Debugging/Assert.h>
#include <Debugging/Logging.h>
#include <Rendering/Window/GlfwWindow.h>
#include <UI/ImGuiBackend.h>

#ifdef WARP_BUILD_DX12
#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12Device.h>
#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12ImGuiBackend.h>
#endif

#ifdef WARP_BUILD_VK
#include <Rendering/Renderer/Platform/Vulkan/VKDevice.h>
#include <Rendering/Renderer/Platform/Vulkan/VKImGuiBackend.h>
#endif

RenderBackend::RenderBackend(GraphicsAPI api)
	: m_api(api)
{
	FATAL_ASSERT(IsSupported(api), "RenderBackend: graphics API not built on this platform");
}

GraphicsAPI RenderBackend::GetStartupAPI()
{
	const GraphicsAPI fallback = IsSupported(GraphicsAPI::D3D12) ? GraphicsAPI::D3D12 : GraphicsAPI::Vulkan;

	const std::optional<String> requested = ConfigFile::GetEngine().Get("r.GraphicsAPI");
	if (!requested)
	{
		return fallback;
	}

	for (GraphicsAPI api : { GraphicsAPI::D3D12, GraphicsAPI::Vulkan })
	{
		if (*requested == GetName(api) && IsSupported(api))
		{
			return api;
		}
	}

	LOG_WARNING("RenderBackend: r.GraphicsAPI = {} is not available here, using {}", *requested, GetName(fallback));
	return fallback;
}

bool RenderBackend::IsSupported(GraphicsAPI api)
{
#ifdef WARP_BUILD_DX12
	constexpr bool bHasD3D12 = true;
#else
	constexpr bool bHasD3D12 = false;
#endif
#ifdef WARP_BUILD_VK
	constexpr bool bHasVulkan = true;
#else
	constexpr bool bHasVulkan = false;
#endif

	return api == GraphicsAPI::D3D12 ? bHasD3D12 : bHasVulkan;
}

const char* RenderBackend::GetName(GraphicsAPI api)
{
	switch (api)
	{
		case GraphicsAPI::D3D12:
			return "D3D12";
		case GraphicsAPI::Vulkan:
			return "Vulkan";
	}
	return "Unknown";
}

URef<IWindow> RenderBackend::MakeWindow(const String& name, int width, int height) const
{
	return std::make_unique<GlfwWindow>(name, width, height);
}

URef<Renderer> RenderBackend::CreateRenderer(IWindow* window) const
{
	// Renderer constructor loads RenderDoc, which must happen before device
	// creation so RenderDoc can hook the graphics API calls.
	URef<Renderer> renderer = std::make_unique<Renderer>();

	DeviceDesc deviceDesc;
	deviceDesc.framesInFlight = Renderer::k_framesInFlight;
#if defined(WARP_DEBUG)
	deviceDesc.bEnableDebugLayer = true;
#endif

	URef<Device> device;
	switch (m_api)
	{
		case GraphicsAPI::D3D12:
#ifdef WARP_BUILD_DX12
			device = std::make_unique<D3D12Device>();
#endif
			break;
		case GraphicsAPI::Vulkan:
#ifdef WARP_BUILD_VK
			device = std::make_unique<VKDevice>();
#endif
			break;
	}

	FATAL_ASSERT(device, "RenderBackend::CreateRenderer: no device for this API");
	device->Initialize(deviceDesc);

	renderer->Init(window, std::move(device));
	LOG_DEBUG("RenderBackend: {} renderer ready", GetName(m_api));
	return renderer;
}

URef<ImGuiBackend> RenderBackend::CreateImGuiBackend() const
{
	switch (m_api)
	{
		case GraphicsAPI::D3D12:
#ifdef WARP_BUILD_DX12
			return std::make_unique<D3D12ImGuiBackend>();
#endif
			break;
		case GraphicsAPI::Vulkan:
#ifdef WARP_BUILD_VK
			return std::make_unique<VKImGuiBackend>();
#endif
			break;
	}
	return nullptr;
}
