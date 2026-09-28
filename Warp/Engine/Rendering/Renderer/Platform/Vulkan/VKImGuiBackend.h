#pragma once

#ifdef WARP_BUILD_VK

#include <UI/ImGuiBackend.h>
#include <Rendering/Renderer/Platform/Vulkan/VKCommon.h>

class VKImGuiBackend : public ImGuiBackend
{
public:
	bool Init(IWindow* window, Device* device, CommandQueue* graphicsQueue, u32 framesInFlight) override;
	void Shutdown() override;
	void NewFrame() override;
	void Render(CommandList* commandList) override;

private:
	VkDevice m_device				  = VK_NULL_HANDLE;
	VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;

	// Must match the swap chain, since ImGui draws straight into the back buffer.
	VkFormat m_colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
};

#endif // WARP_BUILD_VK
