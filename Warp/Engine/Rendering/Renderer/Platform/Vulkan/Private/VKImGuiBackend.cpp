#ifdef WARP_BUILD_VK

#include <Rendering/Renderer/Platform/Vulkan/VKImGuiBackend.h>
#include <Rendering/Renderer/Platform/Vulkan/VKCommandList.h>
#include <Rendering/Renderer/Platform/Vulkan/VKDevice.h>
#include <Rendering/Window/Window.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

bool VKImGuiBackend::Init(IWindow* window, Device* device, CommandQueue* /*graphicsQueue*/, u32 framesInFlight)
{
	VKDevice* vkDevice = static_cast<VKDevice*>(device);
	m_device		   = vkDevice->GetNativeDevice();

	// ImGui's own pool, for the font atlas and any textures the UI shows.
	constexpr u32 k_maxImGuiTextures = 16;

	VkDescriptorPoolSize poolSize	  = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, k_maxImGuiTextures };
	VkDescriptorPoolCreateInfo poolCI = {};
	poolCI.sType					  = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCI.flags					  = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	poolCI.maxSets					  = k_maxImGuiTextures;
	poolCI.poolSizeCount			  = 1;
	poolCI.pPoolSizes				  = &poolSize;
	VK_CHECK(vkCreateDescriptorPool(m_device, &poolCI, nullptr, &m_descriptorPool),
			 "VKImGuiBackend: vkCreateDescriptorPool failed");

	CreateContext(window);

	// install_callbacks chains onto the window's own GLFW callbacks.
	ImGui_ImplGlfw_InitForVulkan(static_cast<GLFWwindow*>(window->GetNativeHandle()), true);

	ImGui_ImplVulkan_InitInfo initInfo						  = {};
	initInfo.Instance										  = vkDevice->GetNativeInstance();
	initInfo.PhysicalDevice									  = vkDevice->GetNativePhysDevice();
	initInfo.Device											  = m_device;
	initInfo.QueueFamily									  = vkDevice->GetGraphicsFamilyIndex();
	initInfo.Queue											  = vkDevice->GetGraphicsQueue();
	initInfo.DescriptorPool									  = m_descriptorPool;
	initInfo.MinImageCount									  = 2;
	initInfo.ImageCount										  = framesInFlight;
	initInfo.MSAASamples									  = VK_SAMPLE_COUNT_1_BIT;
	initInfo.UseDynamicRendering							  = true;
	initInfo.PipelineRenderingCreateInfo.sType				  = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	initInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	initInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &m_colorFormat;

	ImGui_ImplVulkan_Init(&initInfo);
	ImGui_ImplVulkan_CreateFontsTexture();

	return true;
}

void VKImGuiBackend::Shutdown()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	if (m_descriptorPool != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
		m_descriptorPool = VK_NULL_HANDLE;
	}
}

void VKImGuiBackend::NewFrame()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void VKImGuiBackend::Render(CommandList* commandList)
{
	ImGui::Render();

	// Recorded inside the lighting pass's dynamic rendering, onto the back buffer.
	VKCommandList* vkCmd = static_cast<VKCommandList*>(commandList);
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vkCmd->GetNative());
}

#endif // WARP_BUILD_VK
