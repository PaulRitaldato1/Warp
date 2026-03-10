#pragma once

#ifdef WARP_BUILD_VK

#include <Rendering/Renderer/Device.h>
#include <Rendering/Renderer/Platform/Vulkan/VKCommon.h>
#include <vk_mem_alloc.h>

// ---------------------------------------------------------------------------
// VKDevice — owns VkInstance, VkPhysicalDevice, VkDevice, and VmaAllocator.
// All per-API object creation routes through here.
// Targets Vulkan 1.3 (dynamic rendering, synchronization2, timeline semaphores).
// ---------------------------------------------------------------------------

class VKDevice : public Device
{
public:
	~VKDevice() override;

	const char* GetAPIName() override { return "Vulkan"; }

	void Initialize(const DeviceDesc& desc) override;

	URef<CommandQueue>         CreateCommandQueue(CommandQueueType type)         override;
	URef<CommandList>          CreateCommandList(CommandQueueType type, u32 framesInFlight) override;
	URef<UploadBuffer>         CreateUploadBuffer(u64 size, u32 framesInFlight)  override;
	URef<SwapChain>            CreateSwapChain(const SwapChainDesc& desc)        override;
	URef<PipelineState>        CreatePipelineState(const PipelineDesc& desc)     override;
	URef<ComputePipelineState> CreateComputePipelineState(const ComputePipelineDesc& desc) override;
	URef<Buffer>               CreateBuffer(const BufferDesc& desc)              override;
	URef<Texture>              CreateTexture(const TextureDesc& desc)            override;
	URef<Shader>               CreateShader(const ShaderDesc& desc)              override;
	void                       WaitForIdle()                                     override;

	VkInstance       GetNativeInstance()  const { return m_instance; }
	VkPhysicalDevice GetNativePhysDevice()const { return m_physDevice; }
	VkDevice         GetNativeDevice()    const { return m_device; }
	VmaAllocator     GetAllocator()       const { return m_allocator; }

	u32 GetGraphicsFamilyIndex() const { return m_graphicsFamilyIndex; }
	u32 GetComputeFamilyIndex()  const { return m_computeFamilyIndex; }
	u32 GetTransferFamilyIndex() const { return m_transferFamilyIndex; }

	VkQueue   GetGraphicsQueue()  const { return m_graphicsQueue; }
	VkQueue   GetComputeQueue()   const { return m_computeQueue; }
	VkQueue   GetTransferQueue()  const { return m_transferQueue; }
	VkSampler GetDefaultSampler() const { return m_defaultSampler; }

private:
	void PickPhysicalDevice();
	void CreateLogicalDevice(bool enableValidation);
	void CreateAllocator();
	void CreateDefaultSampler();

	VkInstance               m_instance   = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT m_messenger  = VK_NULL_HANDLE;
	VkPhysicalDevice         m_physDevice = VK_NULL_HANDLE;
	VkDevice                 m_device     = VK_NULL_HANDLE;
	VmaAllocator             m_allocator  = VK_NULL_HANDLE;

	VkQueue   m_graphicsQueue  = VK_NULL_HANDLE;
	VkQueue   m_computeQueue   = VK_NULL_HANDLE;
	VkQueue   m_transferQueue  = VK_NULL_HANDLE;
	VkSampler m_defaultSampler = VK_NULL_HANDLE;

	u32 m_graphicsFamilyIndex = 0;
	u32 m_computeFamilyIndex  = 0;
	u32 m_transferFamilyIndex = 0;
};

#endif // WARP_BUILD_VK
