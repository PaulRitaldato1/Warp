#ifdef WARP_BUILD_VK

#include <Rendering/Renderer/Platform/Vulkan/VKPipeline.h>
#include <Rendering/Renderer/Platform/Vulkan/VKShader.h>
#include <Rendering/Renderer/Platform/Vulkan/VKTranslate.h>
#include <Debugging/Assert.h>
#include <Debugging/Logging.h>

// ---------------------------------------------------------------------------
// VKPipeline — graphics PSO with dynamic rendering
// ---------------------------------------------------------------------------

VKPipeline::~VKPipeline()
{
	Cleanup();
}

void VKPipeline::InitializeWithDevice(VkDevice device)
{
	DYNAMIC_ASSERT(device, "VKPipeline: device is null");
	m_device = device;
}

void VKPipeline::Initialize(const PipelineDesc& desc)
{
	DYNAMIC_ASSERT(desc.vertexShader, "VKPipeline: vertexShader is null");
	DYNAMIC_ASSERT(desc.pixelShader, "VKPipeline: pixelShader is null");

	VKShader* vs = static_cast<VKShader*>(desc.vertexShader);
	VKShader* ps = static_cast<VKShader*>(desc.pixelShader);

	// -------------------------------------------------------------------------
	// Shader stages
	// -------------------------------------------------------------------------

	VkPipelineShaderStageCreateInfo stages[2] = {};

	stages[0].sType	 = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage	 = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = vs->GetModule();
	stages[0].pName	 = desc.vertexShader ? "VSMain" : "main";

	stages[1].sType	 = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage	 = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = ps->GetModule();
	stages[1].pName	 = desc.pixelShader ? "PSMain" : "main";

	// -------------------------------------------------------------------------
	// Vertex input — derived from desc.inputLayout; empty = SV_VertexID path
	// -------------------------------------------------------------------------

	Vector<VkVertexInputBindingDescription>   bindings;
	Vector<VkVertexInputAttributeDescription> attributes;

	if (!desc.inputLayout.empty())
	{
		attributes.reserve(desc.inputLayout.size());

		// Per-slot running byte offset for AppendAligned computation.
		u32  slotOffset[16] = {};
		bool slotUsed[16]   = {};

		for (u32 loc = 0; loc < static_cast<u32>(desc.inputLayout.size()); ++loc)
		{
			const InputElement& elem     = desc.inputLayout[loc];
			const u32           slot     = elem.inputSlot;
			const u32           byteSize = FormatByteSize(elem.format);
			const u32           byteOffset =
				(elem.alignedByteOffset == InputElement::AppendAligned) ? slotOffset[slot] : elem.alignedByteOffset;

			VkVertexInputAttributeDescription attr = {};
			attr.location = loc;
			attr.binding  = slot;
			attr.format   = ToVkFormat(elem.format);
			attr.offset   = byteOffset;
			attributes.push_back(attr);

			slotOffset[slot] = byteOffset + byteSize;
			slotUsed[slot]   = true;
		}

		// One binding per referenced slot; stride = accumulated element sizes.
		for (u32 s = 0; s < 16; ++s)
		{
			if (!slotUsed[s])
				continue;

			VkVertexInputBindingDescription binding = {};
			binding.binding   = s;
			binding.stride    = slotOffset[s];
			binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			bindings.push_back(binding);
		}
	}

	VkPipelineVertexInputStateCreateInfo vertexInput = {};
	vertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInput.vertexBindingDescriptionCount   = static_cast<u32>(bindings.size());
	vertexInput.pVertexBindingDescriptions      = bindings.data();
	vertexInput.vertexAttributeDescriptionCount = static_cast<u32>(attributes.size());
	vertexInput.pVertexAttributeDescriptions    = attributes.data();

	// -------------------------------------------------------------------------
	// Input assembly
	// -------------------------------------------------------------------------

	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType									 = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology								 = ToVkTopology(desc.topology);

	// -------------------------------------------------------------------------
	// Viewport / scissor — fully dynamic
	// -------------------------------------------------------------------------

	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType								= VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount						= 1;
	viewportState.scissorCount						= 1;

	// -------------------------------------------------------------------------
	// Rasterizer
	// -------------------------------------------------------------------------

	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType								  = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.polygonMode = (desc.rasterState.fillMode == RasterizerState::FillMode::Wireframe) ? VK_POLYGON_MODE_LINE
																								 : VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth   = 1.f;
	rasterizer.depthClampEnable = desc.rasterState.depthClipEnable ? VK_FALSE : VK_TRUE;

	switch (desc.rasterState.cullMode)
	{
		case RasterizerState::CullMode::None:
			rasterizer.cullMode = VK_CULL_MODE_NONE;
			break;
		case RasterizerState::CullMode::Front:
			rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
			break;
		case RasterizerState::CullMode::Back:
			rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
			break;
	}
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

	// -------------------------------------------------------------------------
	// Multisampling — disabled
	// -------------------------------------------------------------------------

	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType								   = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples				   = VK_SAMPLE_COUNT_1_BIT;

	// -------------------------------------------------------------------------
	// Depth / stencil
	// -------------------------------------------------------------------------

	VkPipelineDepthStencilStateCreateInfo depthStencil = {};
	depthStencil.sType								   = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable					   = desc.enableDepthTest ? VK_TRUE : VK_FALSE;
	depthStencil.depthWriteEnable					   = desc.enableDepthWrite ? VK_TRUE : VK_FALSE;
	depthStencil.depthCompareOp						   = VK_COMPARE_OP_LESS;
	depthStencil.stencilTestEnable					   = desc.enableStencilTest ? VK_TRUE : VK_FALSE;

	// -------------------------------------------------------------------------
	// Colour blend — one attachment per render target
	// -------------------------------------------------------------------------

	Vector<VkPipelineColorBlendAttachmentState> blendAttachments;
	blendAttachments.reserve(desc.renderTargetFormats.size());

	for (size_t i = 0; i < desc.renderTargetFormats.size(); ++i)
	{
		VkPipelineColorBlendAttachmentState att = {};
		att.colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		if (desc.enableBlending)
		{
			att.blendEnable			= VK_TRUE;
			att.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			att.colorBlendOp		= VK_BLEND_OP_ADD;
			att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			att.alphaBlendOp		= VK_BLEND_OP_ADD;
		}

		blendAttachments.push_back(att);
	}

	VkPipelineColorBlendStateCreateInfo colourBlend = {};
	colourBlend.sType								= VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colourBlend.attachmentCount						= static_cast<u32>(blendAttachments.size());
	colourBlend.pAttachments						= blendAttachments.data();

	// -------------------------------------------------------------------------
	// Dynamic state (viewport + scissor change every frame)
	// -------------------------------------------------------------------------

	const VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};

	VkPipelineDynamicStateCreateInfo dynamicState = {};
	dynamicState.sType							  = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount				  = 2;
	dynamicState.pDynamicStates					  = dynamicStates;

	// -------------------------------------------------------------------------
	// Descriptor set layout — built from PipelineDesc::bindings.
	//
	// Each BindingSlot maps to one or more sequential Vulkan bindings:
	//   ConstantBuffer   → 1 UBO binding
	//   TextureTable     → N combined-image-sampler bindings
	//   StructuredBuffer → 1 SSBO binding
	// -------------------------------------------------------------------------

	{
		Vector<VkDescriptorSetLayoutBinding> vkBindings;
		m_rootToVulkanBinding.resize(desc.bindings.size());
		u32 vulkanBindingIndex = 0;

		for (u32 rootIndex = 0; rootIndex < static_cast<u32>(desc.bindings.size()); ++rootIndex)
		{
			const BindingSlot& slot = desc.bindings[rootIndex];
			m_rootToVulkanBinding[rootIndex] = vulkanBindingIndex;

			switch (slot.type)
			{
				case BindingType::ConstantBuffer:
				{
					VkDescriptorSetLayoutBinding binding = {};
					binding.binding         = vulkanBindingIndex++;
					binding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
					binding.descriptorCount = 1;
					binding.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
					vkBindings.push_back(binding);
					break;
				}
				case BindingType::TextureTable:
				{
					for (u32 texIndex = 0; texIndex < slot.count; ++texIndex)
					{
						VkDescriptorSetLayoutBinding binding = {};
						binding.binding         = vulkanBindingIndex++;
						binding.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
						binding.descriptorCount = 1;
						binding.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
						vkBindings.push_back(binding);
					}
					break;
				}
				case BindingType::StructuredBuffer:
				{
					VkDescriptorSetLayoutBinding binding = {};
					binding.binding         = vulkanBindingIndex++;
					binding.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
					binding.descriptorCount = 1;
					binding.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
					vkBindings.push_back(binding);
					break;
				}
			}
		}

		VkDescriptorSetLayoutCreateInfo setLayoutInfo = {};
		setLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		setLayoutInfo.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
		setLayoutInfo.bindingCount = static_cast<u32>(vkBindings.size());
		setLayoutInfo.pBindings    = vkBindings.data();

		VK_CHECK(vkCreateDescriptorSetLayout(m_device, &setLayoutInfo, nullptr, &m_descriptorSetLayout),
				 "VKPipeline: vkCreateDescriptorSetLayout failed");
	}

	// -------------------------------------------------------------------------
	// Pipeline layout
	// -------------------------------------------------------------------------

	VkPipelineLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.setLayoutCount = 1;
	layoutInfo.pSetLayouts    = &m_descriptorSetLayout;

	VK_CHECK(vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_layout),
			 "VKPipeline: vkCreatePipelineLayout failed");

	// -------------------------------------------------------------------------
	// Dynamic rendering — attach colour/depth format info via pNext chain
	// -------------------------------------------------------------------------

	Vector<VkFormat> colourFormats;
	colourFormats.reserve(desc.renderTargetFormats.size());
	for (TextureFormat fmt : desc.renderTargetFormats)
	{
		colourFormats.push_back(ToVkFormat(fmt));
	}

	VkFormat depthFmt =
		(desc.depthFormat != TextureFormat::Unknown) ? ToVkFormat(desc.depthFormat) : VK_FORMAT_UNDEFINED;

	VkPipelineRenderingCreateInfoKHR renderingInfo = {};
	renderingInfo.sType							   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
	renderingInfo.colorAttachmentCount			   = static_cast<u32>(colourFormats.size());
	renderingInfo.pColorAttachmentFormats		   = colourFormats.data();
	renderingInfo.depthAttachmentFormat			   = depthFmt;

	// -------------------------------------------------------------------------
	// Graphics pipeline
	// -------------------------------------------------------------------------

	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType						  = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.pNext						  = &renderingInfo;
	pipelineInfo.stageCount					  = 2;
	pipelineInfo.pStages					  = stages;
	pipelineInfo.pVertexInputState			  = &vertexInput;
	pipelineInfo.pInputAssemblyState		  = &inputAssembly;
	pipelineInfo.pViewportState				  = &viewportState;
	pipelineInfo.pRasterizationState		  = &rasterizer;
	pipelineInfo.pMultisampleState			  = &multisampling;
	pipelineInfo.pDepthStencilState			  = &depthStencil;
	pipelineInfo.pColorBlendState			  = &colourBlend;
	pipelineInfo.pDynamicState				  = &dynamicState;
	pipelineInfo.layout						  = m_layout;
	pipelineInfo.renderPass					  = VK_NULL_HANDLE; // dynamic rendering — no render pass

	VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline),
			 "VKPipeline: vkCreateGraphicsPipelines failed");

	LOG_DEBUG("VKPipeline: graphics PSO created");
}

void VKPipeline::Cleanup()
{
	if (m_pipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(m_device, m_pipeline, nullptr);
		m_pipeline = VK_NULL_HANDLE;
	}
	if (m_layout != VK_NULL_HANDLE)
	{
		vkDestroyPipelineLayout(m_device, m_layout, nullptr);
		m_layout = VK_NULL_HANDLE;
	}
	if (m_descriptorSetLayout != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
		m_descriptorSetLayout = VK_NULL_HANDLE;
	}
}

// ---------------------------------------------------------------------------
// VKComputePipeline
// ---------------------------------------------------------------------------

VKComputePipeline::~VKComputePipeline()
{
	Cleanup();
}

void VKComputePipeline::InitializeWithDevice(VkDevice device)
{
	m_device = device;
}

void VKComputePipeline::Initialize(const ComputePipelineDesc& desc)
{
	DYNAMIC_ASSERT(desc.computeShader, "VKComputePipeline: computeShader is null");
	VKShader* cs = static_cast<VKShader*>(desc.computeShader);

	VkPipelineLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType					  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	VK_CHECK(vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_layout),
			 "VKComputePipeline: vkCreatePipelineLayout failed");

	VkComputePipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType						 = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineInfo.layout						 = m_layout;
	pipelineInfo.stage.sType				 = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pipelineInfo.stage.stage				 = VK_SHADER_STAGE_COMPUTE_BIT;
	pipelineInfo.stage.module				 = cs->GetModule();
	pipelineInfo.stage.pName				 = "CSMain";

	VK_CHECK(vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline),
			 "VKComputePipeline: vkCreateComputePipelines failed");
}

void VKComputePipeline::Cleanup()
{
	if (m_pipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(m_device, m_pipeline, nullptr);
		m_pipeline = VK_NULL_HANDLE;
	}
	if (m_layout != VK_NULL_HANDLE)
	{
		vkDestroyPipelineLayout(m_device, m_layout, nullptr);
		m_layout = VK_NULL_HANDLE;
	}
}

#endif // WARP_BUILD_VK
