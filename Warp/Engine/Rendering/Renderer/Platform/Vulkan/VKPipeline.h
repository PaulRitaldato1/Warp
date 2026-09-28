#pragma once

#ifdef WARP_BUILD_VK

#include <Rendering/Renderer/Pipeline.h>
#include <Rendering/Renderer/Platform/Vulkan/VKCommon.h>

// A sampler the pipeline owns, pushed into its descriptor set whenever the
// pipeline is bound. Plain pushed samplers rather than immutable ones, since
// immutable samplers inside a push descriptor set are rare enough that capture
// tools like RenderDoc do not track them.
struct VKSamplerBinding
{
	u32 binding		  = 0; // shift S + HLSL register
	VkSampler sampler = VK_NULL_HANDLE;
};

// ---------------------------------------------------------------------------
// VKPipeline — graphics PSO built with dynamic rendering (no render passes).
// ---------------------------------------------------------------------------

class VKPipeline : public PipelineState
{
public:
	~VKPipeline() override;

	void InitializeWithDevice(VkDevice device);

	void Initialize(const PipelineDesc& desc) override;
	void Cleanup() override;
	void* GetNativeHandle() const override
	{
		return (void*)m_pipeline;
	}

	VkPipeline GetNativePipeline() const
	{
		return m_pipeline;
	}
	VkPipelineLayout GetNativeLayout() const
	{
		return m_layout;
	}
	VkDescriptorSetLayout GetDescriptorSetLayout() const
	{
		return m_descriptorSetLayout;
	}

	// Maps rootIndex (CommandList binding slot) to the Vulkan descriptor set binding index.
	// Needed because a TextureTable with N textures expands to N sequential Vulkan bindings.
	const Vector<u32>& GetRootToVulkanBindingMap() const
	{
		return m_rootToVulkanBinding;
	}

	// Pushed by VKCommandList::SetPipelineState.
	const Vector<VKSamplerBinding>& GetSamplerBindings() const
	{
		return m_samplers;
	}

private:
	// Native handles
	VkDevice m_device							= VK_NULL_HANDLE;
	VkPipeline m_pipeline						= VK_NULL_HANDLE;
	VkPipelineLayout m_layout					= VK_NULL_HANDLE;
	VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;

	// rootIndex -> first Vulkan binding index for that slot.
	Vector<u32> m_rootToVulkanBinding;
	Vector<VKSamplerBinding> m_samplers;
};

// ---------------------------------------------------------------------------
// VKComputePipeline
// ---------------------------------------------------------------------------

class VKComputePipeline : public ComputePipelineState
{
public:
	~VKComputePipeline() override;

	void InitializeWithDevice(VkDevice device);

	void Initialize(const ComputePipelineDesc& desc) override;
	void Cleanup() override;
	void* GetNativeHandle() const override
	{
		return (void*)m_pipeline;
	}

	VkPipeline GetNativePipeline() const
	{
		return m_pipeline;
	}
	VkPipelineLayout GetNativeLayout() const
	{
		return m_layout;
	}

	const Vector<u32>& GetRootToVulkanBindingMap() const
	{
		return m_rootToVulkanBinding;
	}

private:
	VkDevice m_device							= VK_NULL_HANDLE;
	VkPipeline m_pipeline						= VK_NULL_HANDLE;
	VkPipelineLayout m_layout					= VK_NULL_HANDLE;
	VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;

	Vector<u32> m_rootToVulkanBinding;
};

#endif // WARP_BUILD_VK
