#pragma once

#ifdef WARP_LINUX

#include <UI/ImGuiBackend.h>
#include <vulkan/vulkan_core.h>

class VKImGuiBackend : public ImGuiBackend
{
public:
	bool Init(IWindow* window, Device* device, CommandQueue* graphicsQueue, u32 framesInFlight) override;
	void Shutdown() override;
	void NewFrame() override;
	void Render(CommandList* commandList) override;

private:
	VkDevice         m_device         = VK_NULL_HANDLE;
	VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
	VkFormat         m_colorFormat    = VK_FORMAT_B8G8R8A8_UNORM;
};

#endif // WARP_LINUX
