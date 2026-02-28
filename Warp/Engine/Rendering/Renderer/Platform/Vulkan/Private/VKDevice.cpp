#ifdef WARP_BUILD_VK

#include <Rendering/Renderer/Platform/Vulkan/VKDevice.h>
#include <Rendering/Renderer/Platform/Vulkan/VKCommandQueue.h>
#include <Rendering/Renderer/Platform/Vulkan/VKCommandList.h>
#include <Rendering/Renderer/Platform/Vulkan/VKSwapChain.h>
#include <Rendering/Renderer/Platform/Vulkan/VKPipeline.h>
#include <Rendering/Renderer/Platform/Vulkan/VKBuffer.h>
#include <Rendering/Renderer/Platform/Vulkan/VKTexture.h>
#include <Rendering/Renderer/Platform/Vulkan/VKShader.h>
#include <Rendering/Renderer/Platform/Vulkan/VKUploadBuffer.h>
#include <Debugging/Assert.h>
#include <Debugging/Logging.h>
#include <vector>
#include <cstring>

// ---------------------------------------------------------------------------
// Debug messenger callback
// ---------------------------------------------------------------------------

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
	VkDebugUtilsMessageTypeFlagsEXT             /*type*/,
	const VkDebugUtilsMessengerCallbackDataEXT* data,
	void*                                       /*pUser*/)
{
	if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
	{
		LOG_ERROR(String("[Vulkan] ") + data->pMessage);
	}
	else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
	{
		LOG_WARNING(String("[Vulkan] ") + data->pMessage);
	}
	else
	{
		LOG_DEBUG(String("[Vulkan] ") + data->pMessage);
	}
	return VK_FALSE;
}

// ---------------------------------------------------------------------------
// VKDevice
// ---------------------------------------------------------------------------

VKDevice::~VKDevice()
{
	if (m_allocator)
	{
		vmaDestroyAllocator(m_allocator);
		m_allocator = VK_NULL_HANDLE;
	}
	if (m_device)
	{
		vkDestroyDevice(m_device, nullptr);
		m_device = VK_NULL_HANDLE;
	}
	if (m_messenger)
	{
		auto destroyFn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
			vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
		if (destroyFn)
		{
			destroyFn(m_instance, m_messenger, nullptr);
		}
		m_messenger = VK_NULL_HANDLE;
	}
	if (m_instance)
	{
		vkDestroyInstance(m_instance, nullptr);
		m_instance = VK_NULL_HANDLE;
	}
}

void VKDevice::Initialize(const DeviceDesc& desc)
{
	// -------------------------------------------------------------------------
	// Instance
	// -------------------------------------------------------------------------

	VkApplicationInfo appInfo = {};
	appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName   = "Warp";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName        = "WarpEngine";
	appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion         = VK_API_VERSION_1_3;

	std::vector<const char*> instExtensions = {
		VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef WARP_LINUX
		VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#endif
	};

	std::vector<const char*> layers;

	VkDebugUtilsMessengerCreateInfoEXT messengerInfo = {};

	if (desc.bEnableDebugLayer)
	{
		instExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		layers.push_back("VK_LAYER_KHRONOS_validation");

		messengerInfo.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		messengerInfo.messageSeverity =
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		messengerInfo.messageType     =
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT     |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT  |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		messengerInfo.pfnUserCallback = DebugCallback;
	}

	VkInstanceCreateInfo instInfo = {};
	instInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instInfo.pNext                   = desc.bEnableDebugLayer ? &messengerInfo : nullptr;
	instInfo.pApplicationInfo        = &appInfo;
	instInfo.enabledExtensionCount   = static_cast<u32>(instExtensions.size());
	instInfo.ppEnabledExtensionNames = instExtensions.data();
	instInfo.enabledLayerCount       = static_cast<u32>(layers.size());
	instInfo.ppEnabledLayerNames     = layers.data();

	VK_CHECK(vkCreateInstance(&instInfo, nullptr, &m_instance),
	         "VKDevice: vkCreateInstance failed");

	// -------------------------------------------------------------------------
	// Debug messenger
	// -------------------------------------------------------------------------

	if (desc.bEnableDebugLayer)
	{
		auto createFn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
			vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));

		if (createFn)
		{
			createFn(m_instance, &messengerInfo, nullptr, &m_messenger);
		}
		else
		{
			LOG_WARNING("VKDevice: VK_EXT_debug_utils not available; no debug callback");
		}
	}

	// -------------------------------------------------------------------------
	// Physical device
	// -------------------------------------------------------------------------

	PickPhysicalDevice();

	// -------------------------------------------------------------------------
	// Logical device
	// -------------------------------------------------------------------------

	CreateLogicalDevice(desc.bEnableDebugLayer);

	// -------------------------------------------------------------------------
	// VMA allocator
	// -------------------------------------------------------------------------

	CreateAllocator();

	LOG_DEBUG("VKDevice initialized");
}

void VKDevice::PickPhysicalDevice()
{
	u32 deviceCount = 0;
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
	DYNAMIC_ASSERT(deviceCount > 0, "VKDevice: no Vulkan-capable GPU found");

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

	// Prefer discrete GPU; otherwise take the first available.
	m_physDevice = devices[0];
	for (auto dev : devices)
	{
		VkPhysicalDeviceProperties props = {};
		vkGetPhysicalDeviceProperties(dev, &props);
		if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			m_physDevice = dev;
			break;
		}
	}

	VkPhysicalDeviceProperties chosen = {};
	vkGetPhysicalDeviceProperties(m_physDevice, &chosen);
	LOG_DEBUG(String("VKDevice: selected GPU — ") + chosen.deviceName);
}

void VKDevice::CreateLogicalDevice(bool enableValidation)
{
	// -------------------------------------------------------------------------
	// Queue families
	// -------------------------------------------------------------------------

	u32 familyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(m_physDevice, &familyCount, nullptr);
	std::vector<VkQueueFamilyProperties> families(familyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(m_physDevice, &familyCount, families.data());

	m_graphicsFamilyIndex = UINT32_MAX;
	m_computeFamilyIndex  = UINT32_MAX;
	m_transferFamilyIndex = UINT32_MAX;

	for (u32 i = 0; i < familyCount; ++i)
	{
		VkQueueFlags flags = families[i].queueFlags;

		if ((flags & VK_QUEUE_GRAPHICS_BIT) && m_graphicsFamilyIndex == UINT32_MAX)
		{
			m_graphicsFamilyIndex = i;
		}
		if ((flags & VK_QUEUE_COMPUTE_BIT) && !(flags & VK_QUEUE_GRAPHICS_BIT) &&
		    m_computeFamilyIndex == UINT32_MAX)
		{
			m_computeFamilyIndex = i;
		}
		if ((flags & VK_QUEUE_TRANSFER_BIT) && !(flags & VK_QUEUE_GRAPHICS_BIT) &&
		    !(flags & VK_QUEUE_COMPUTE_BIT) && m_transferFamilyIndex == UINT32_MAX)
		{
			m_transferFamilyIndex = i;
		}
	}

	DYNAMIC_ASSERT(m_graphicsFamilyIndex != UINT32_MAX, "VKDevice: no graphics queue family");

	// Fall back to graphics family if dedicated compute/transfer aren't available.
	if (m_computeFamilyIndex == UINT32_MAX)
	{
		m_computeFamilyIndex = m_graphicsFamilyIndex;
	}
	if (m_transferFamilyIndex == UINT32_MAX)
	{
		m_transferFamilyIndex = m_graphicsFamilyIndex;
	}

	// Build unique queue-create infos.
	const float priority = 1.f;
	std::vector<VkDeviceQueueCreateInfo> queueInfos;
	std::vector<u32> uniqueFamilies;

	auto addFamily = [&](u32 idx)
	{
		for (u32 f : uniqueFamilies)
		{
			if (f == idx)
			{
				return;
			}
		}
		uniqueFamilies.push_back(idx);

		VkDeviceQueueCreateInfo qi = {};
		qi.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		qi.queueFamilyIndex = idx;
		qi.queueCount       = 1;
		qi.pQueuePriorities = &priority;
		queueInfos.push_back(qi);
	};

	addFamily(m_graphicsFamilyIndex);
	addFamily(m_computeFamilyIndex);
	addFamily(m_transferFamilyIndex);

	// -------------------------------------------------------------------------
	// Device features — require Vulkan 1.3 core features
	// -------------------------------------------------------------------------

	VkPhysicalDeviceVulkan13Features features13 = {};
	features13.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	features13.dynamicRendering = VK_TRUE;
	features13.synchronization2 = VK_TRUE;

	VkPhysicalDeviceVulkan12Features features12 = {};
	features12.sType                = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	features12.pNext                = &features13;
	features12.timelineSemaphore    = VK_TRUE;
	features12.bufferDeviceAddress  = VK_TRUE;

	VkPhysicalDeviceFeatures2 features2 = {};
	features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features2.pNext = &features12;

	// -------------------------------------------------------------------------
	// Device extensions
	// -------------------------------------------------------------------------

	std::vector<const char*> devExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	};

	std::vector<const char*> devLayers;
	if (enableValidation)
	{
		devLayers.push_back("VK_LAYER_KHRONOS_validation");
	}

	VkDeviceCreateInfo deviceInfo = {};
	deviceInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceInfo.pNext                   = &features2;
	deviceInfo.queueCreateInfoCount    = static_cast<u32>(queueInfos.size());
	deviceInfo.pQueueCreateInfos       = queueInfos.data();
	deviceInfo.enabledExtensionCount   = static_cast<u32>(devExtensions.size());
	deviceInfo.ppEnabledExtensionNames = devExtensions.data();
	deviceInfo.enabledLayerCount       = static_cast<u32>(devLayers.size());
	deviceInfo.ppEnabledLayerNames     = devLayers.data();

	VK_CHECK(vkCreateDevice(m_physDevice, &deviceInfo, nullptr, &m_device),
	         "VKDevice: vkCreateDevice failed");

	vkGetDeviceQueue(m_device, m_graphicsFamilyIndex, 0, &m_graphicsQueue);
	vkGetDeviceQueue(m_device, m_computeFamilyIndex,  0, &m_computeQueue);
	vkGetDeviceQueue(m_device, m_transferFamilyIndex, 0, &m_transferQueue);
}

void VKDevice::CreateAllocator()
{
	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice   = m_physDevice;
	allocatorInfo.device           = m_device;
	allocatorInfo.instance         = m_instance;
	allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
	allocatorInfo.flags            = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

	VK_CHECK(vmaCreateAllocator(&allocatorInfo, &m_allocator),
	         "VKDevice: vmaCreateAllocator failed");
}

// ---------------------------------------------------------------------------
// Factory methods
// ---------------------------------------------------------------------------

URef<CommandQueue> VKDevice::CreateCommandQueue(CommandQueueType type)
{
	VkQueue queue       = VK_NULL_HANDLE;
	u32     familyIndex = 0;

	switch (type)
	{
		case CommandQueueType::Graphics:
			queue       = m_graphicsQueue;
			familyIndex = m_graphicsFamilyIndex;
			break;
		case CommandQueueType::Compute:
			queue       = m_computeQueue;
			familyIndex = m_computeFamilyIndex;
			break;
		case CommandQueueType::Copy:
			queue       = m_transferQueue;
			familyIndex = m_transferFamilyIndex;
			break;
	}

	auto vkQueue = std::make_unique<VKCommandQueue>();
	vkQueue->InitializeWithDevice(m_device, queue, familyIndex);
	return vkQueue;
}

URef<CommandList> VKDevice::CreateCommandList(CommandQueueType type, u32 framesInFlight)
{
	u32 familyIndex = 0;
	switch (type)
	{
		case CommandQueueType::Graphics: familyIndex = m_graphicsFamilyIndex; break;
		case CommandQueueType::Compute:  familyIndex = m_computeFamilyIndex;  break;
		case CommandQueueType::Copy:     familyIndex = m_transferFamilyIndex; break;
	}

	auto list = std::make_unique<VKCommandList>();
	list->InitializeWithDevice(m_device, familyIndex, framesInFlight);
	return list;
}

URef<UploadBuffer> VKDevice::CreateUploadBuffer(u64 size, u32 framesInFlight)
{
	auto buf = std::make_unique<VKUploadBuffer>();
	buf->InitializeWithAllocator(m_allocator, m_device);
	buf->Initialize(size, framesInFlight);
	return buf;
}

URef<SwapChain> VKDevice::CreateSwapChain(const SwapChainDesc& desc)
{
	auto sc = std::make_unique<VKSwapChain>();
	sc->InitializeWithContext(m_instance, m_physDevice, m_device,
	                          m_graphicsQueue, m_graphicsFamilyIndex);
	sc->Initialize(desc);
	return sc;
}

URef<PipelineState> VKDevice::CreatePipelineState(const PipelineDesc& desc)
{
	auto pso = std::make_unique<VKPipeline>();
	pso->InitializeWithDevice(m_device);
	pso->Initialize(desc);
	return pso;
}

URef<ComputePipelineState> VKDevice::CreateComputePipelineState(const ComputePipelineDesc& desc)
{
	auto pso = std::make_unique<VKComputePipeline>();
	pso->InitializeWithDevice(m_device);
	pso->Initialize(desc);
	return pso;
}

URef<Buffer> VKDevice::CreateBuffer(const BufferDesc& desc)
{
	auto buf = std::make_unique<VKBuffer>();
	buf->InitializeWithAllocator(m_allocator, m_device);
	buf->CreateBuffer(desc);
	return buf;
}

URef<Texture> VKDevice::CreateTexture(const TextureDesc& desc)
{
	auto tex = std::make_unique<VKTexture>();
	tex->InitializeWithAllocator(m_allocator, m_device);
	tex->Init(desc);
	return tex;
}

URef<Shader> VKDevice::CreateShader(const ShaderDesc& desc)
{
	auto shader = std::make_unique<VKShader>();
	shader->InitializeWithDevice(m_device);
	shader->Initialize(desc);
	return shader;
}

void VKDevice::WaitForIdle()
{
	vkDeviceWaitIdle(m_device);
}

#endif // WARP_BUILD_VK
