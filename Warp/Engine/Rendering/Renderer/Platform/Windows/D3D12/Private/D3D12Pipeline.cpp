#ifdef WARP_BUILD_DX12

#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12Pipeline.h>
#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12Shader.h>
#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12Translate.h>
#include <Debugging/Assert.h>
#include <Debugging/Logging.h>

// ===========================================================================
// D3D12Pipeline — graphics PSO
// ===========================================================================

// ---------------------------------------------------------------------------
// Data-driven root signature — built from PipelineDesc::bindings.
//
// Each BindingSlot becomes one root parameter:
//   ConstantBuffer   → root CBV  at register bN
//   TextureTable     → descriptor table of N SRVs starting at register tN
//   StructuredBuffer → root SRV  at register tN
//
// Static samplers are always present:
//   s0: linear wrap   (material textures)
//   s1: point clamp   (GBuffer sampling)
// ---------------------------------------------------------------------------

void D3D12Pipeline::BuildRootSignature(ID3D12Device* device, const Vector<BindingSlot>& bindings)
{
	// Build root parameters from binding slots.
	// Descriptor ranges must outlive the root signature creation call,
	// so store them alongside their root parameters.
	Vector<D3D12_ROOT_PARAMETER>  params(bindings.size());
	Vector<D3D12_DESCRIPTOR_RANGE> ranges(bindings.size());

	for (u32 index = 0; index < static_cast<u32>(bindings.size()); ++index)
	{
		const BindingSlot& slot = bindings[index];

		switch (slot.type)
		{
			case BindingType::ConstantBuffer:
			{
				params[index].ParameterType				= D3D12_ROOT_PARAMETER_TYPE_CBV;
				params[index].Descriptor.ShaderRegister = slot.shaderRegister;
				params[index].Descriptor.RegisterSpace	= 0;
				params[index].ShaderVisibility			= D3D12_SHADER_VISIBILITY_ALL;
				break;
			}
			case BindingType::TextureTable:
			{
				ranges[index].RangeType							= D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				ranges[index].NumDescriptors					= slot.count;
				ranges[index].BaseShaderRegister				= slot.shaderRegister;
				ranges[index].RegisterSpace						= 0;
				ranges[index].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

				params[index].ParameterType						  = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				params[index].DescriptorTable.NumDescriptorRanges = 1;
				params[index].DescriptorTable.pDescriptorRanges	  = &ranges[index];
				params[index].ShaderVisibility					  = D3D12_SHADER_VISIBILITY_PIXEL;
				break;
			}
			case BindingType::StructuredBuffer:
			{
				params[index].ParameterType				= D3D12_ROOT_PARAMETER_TYPE_SRV;
				params[index].Descriptor.ShaderRegister = slot.shaderRegister;
				params[index].Descriptor.RegisterSpace	= 0;
				params[index].ShaderVisibility			= D3D12_SHADER_VISIBILITY_ALL;
				break;
			}
		}
	}

	// Static samplers — always present regardless of binding layout.
	D3D12_STATIC_SAMPLER_DESC samplers[2] = {};
	// s0: linear wrap
	samplers[0].Filter			 = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samplers[0].AddressU		 = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplers[0].AddressV		 = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplers[0].AddressW		 = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplers[0].MaxAnisotropy	 = 1;
	samplers[0].ComparisonFunc	 = D3D12_COMPARISON_FUNC_ALWAYS;
	samplers[0].MaxLOD			 = D3D12_FLOAT32_MAX;
	samplers[0].ShaderRegister	 = 0; // s0
	samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	// s1: point clamp
	samplers[1].Filter			 = D3D12_FILTER_MIN_MAG_MIP_POINT;
	samplers[1].AddressU		 = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplers[1].AddressV		 = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplers[1].AddressW		 = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplers[1].MaxAnisotropy	 = 1;
	samplers[1].ComparisonFunc	 = D3D12_COMPARISON_FUNC_ALWAYS;
	samplers[1].MaxLOD			 = D3D12_FLOAT32_MAX;
	samplers[1].ShaderRegister	 = 1; // s1
	samplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
	rootSigDesc.NumParameters			  = static_cast<UINT>(params.size());
	rootSigDesc.pParameters				  = params.data();
	rootSigDesc.NumStaticSamplers		  = _countof(samplers);
	rootSigDesc.pStaticSamplers			  = samplers;
	rootSigDesc.Flags					  = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ComRef<ID3DBlob> serialized, errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errorBlob);
	if (errorBlob)
	{
		LOG_WARNING("D3D12Pipeline root sig serialization: {}", static_cast<char*>(errorBlob->GetBufferPointer()));
	}
	ThrowIfFailed(hr);
	ThrowIfFailed(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
											  IID_PPV_ARGS(&m_rootSignature)));
}

void D3D12Pipeline::InitializeWithDevice(ID3D12Device* device, const PipelineDesc& desc)
{
	DYNAMIC_ASSERT(device, "D3D12Pipeline::InitializeWithDevice: device is null");
	DYNAMIC_ASSERT(desc.vertexShader, "D3D12Pipeline::InitializeWithDevice: vertexShader required");
	DYNAMIC_ASSERT(desc.renderTargetFormats.size() <= 8, "D3D12Pipeline: max 8 render target formats");

	BuildRootSignature(device, desc.bindings);

	// --- Input layout ---
	// SemanticName pointers stay valid until CreateGraphicsPipelineState returns,
	// which is synchronous.  The strings live in desc.inputLayout (caller's scope).
	Vector<D3D12_INPUT_ELEMENT_DESC> d3dElements;
	d3dElements.reserve(desc.inputLayout.size());
	for (const InputElement& elem : desc.inputLayout)
	{
		D3D12_INPUT_ELEMENT_DESC d = {};
		d.SemanticName			   = elem.semanticName.c_str();
		d.SemanticIndex			   = elem.semanticIndex;
		d.Format				   = ToD3D12Format(elem.format);
		d.InputSlot				   = elem.inputSlot;
		d.AlignedByteOffset	   = (elem.alignedByteOffset == InputElement::AppendAligned) ? D3D12_APPEND_ALIGNED_ELEMENT
																						 : elem.alignedByteOffset;
		d.InputSlotClass	   = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		d.InstanceDataStepRate = 0;
		d3dElements.push_back(d);
	}

	// --- Rasterizer ---
	D3D12_RASTERIZER_DESC rast = {};
	rast.FillMode = (desc.rasterState.fillMode == RasterizerState::FillMode::Wireframe) ? D3D12_FILL_MODE_WIREFRAME
																						: D3D12_FILL_MODE_SOLID;
	switch (desc.rasterState.cullMode)
	{
		case RasterizerState::CullMode::None:
			rast.CullMode = D3D12_CULL_MODE_NONE;
			break;
		case RasterizerState::CullMode::Front:
			rast.CullMode = D3D12_CULL_MODE_FRONT;
			break;
		default:
			rast.CullMode = D3D12_CULL_MODE_BACK;
			break;
	}
	rast.FrontCounterClockwise = FALSE;
	rast.DepthClipEnable	   = desc.rasterState.depthClipEnable ? TRUE : FALSE;
	rast.DepthBias			   = D3D12_DEFAULT_DEPTH_BIAS;
	rast.DepthBiasClamp		   = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	rast.SlopeScaledDepthBias  = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	rast.MultisampleEnable	   = FALSE;
	rast.AntialiasedLineEnable = FALSE;

	// --- Blend ---
	auto ToD3DBlendFactor = [](BlendState::BlendFactor f) -> D3D12_BLEND
	{
		switch (f)
		{
			case BlendState::BlendFactor::Zero:
				return D3D12_BLEND_ZERO;
			case BlendState::BlendFactor::One:
				return D3D12_BLEND_ONE;
			case BlendState::BlendFactor::SrcColor:
				return D3D12_BLEND_SRC_COLOR;
			case BlendState::BlendFactor::InvSrcColor:
				return D3D12_BLEND_INV_SRC_COLOR;
			case BlendState::BlendFactor::SrcAlpha:
				return D3D12_BLEND_SRC_ALPHA;
			case BlendState::BlendFactor::InvSrcAlpha:
				return D3D12_BLEND_INV_SRC_ALPHA;
			case BlendState::BlendFactor::DestAlpha:
				return D3D12_BLEND_DEST_ALPHA;
			case BlendState::BlendFactor::InvDestAlpha:
				return D3D12_BLEND_INV_DEST_ALPHA;
			case BlendState::BlendFactor::DestColor:
				return D3D12_BLEND_DEST_COLOR;
			case BlendState::BlendFactor::InvDestColor:
				return D3D12_BLEND_INV_DEST_COLOR;
			default:
				return D3D12_BLEND_ONE;
		}
	};
	auto ToD3DBlendOp = [](BlendState::BlendOp op) -> D3D12_BLEND_OP
	{
		switch (op)
		{
			case BlendState::BlendOp::Subtract:
				return D3D12_BLEND_OP_SUBTRACT;
			case BlendState::BlendOp::ReverseSubtract:
				return D3D12_BLEND_OP_REV_SUBTRACT;
			case BlendState::BlendOp::Min:
				return D3D12_BLEND_OP_MIN;
			case BlendState::BlendOp::Max:
				return D3D12_BLEND_OP_MAX;
			default:
				return D3D12_BLEND_OP_ADD;
		}
	};

	D3D12_BLEND_DESC blend				= {};
	blend.AlphaToCoverageEnable			= FALSE;
	blend.IndependentBlendEnable		= FALSE;
	D3D12_RENDER_TARGET_BLEND_DESC& rtb = blend.RenderTarget[0];
	rtb.BlendEnable						= desc.enableBlending ? TRUE : FALSE;
	rtb.SrcBlend						= ToD3DBlendFactor(desc.blend.srcBlend);
	rtb.DestBlend						= ToD3DBlendFactor(desc.blend.destBlend);
	rtb.BlendOp							= ToD3DBlendOp(desc.blend.blendOp);
	rtb.SrcBlendAlpha					= ToD3DBlendFactor(desc.blend.srcBlendAlpha);
	rtb.DestBlendAlpha					= ToD3DBlendFactor(desc.blend.destBlendAlpha);
	rtb.BlendOpAlpha					= ToD3DBlendOp(desc.blend.blendOpAlpha);
	rtb.RenderTargetWriteMask			= D3D12_COLOR_WRITE_ENABLE_ALL;

	// --- Depth-stencil ---
	// enableDepthWrite=false + enableDepthTest=true → read-only depth (transparency, overlay).
	// enableDepthTest=false → depth buffer ignored entirely (2D UI, fullscreen effects).
	D3D12_DEPTH_STENCIL_DESC ds = {};
	ds.DepthEnable				= desc.enableDepthTest ? TRUE : FALSE;
	ds.DepthWriteMask			= desc.enableDepthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
	ds.DepthFunc				= D3D12_COMPARISON_FUNC_LESS;
	ds.StencilEnable			= desc.enableStencilTest ? TRUE : FALSE;

	// --- PSO ---
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature					   = m_rootSignature.Get();
	psoDesc.InputLayout						   = { d3dElements.data(), static_cast<UINT>(d3dElements.size()) };
	psoDesc.RasterizerState					   = rast;
	psoDesc.BlendState						   = blend;
	psoDesc.DepthStencilState				   = ds;
	psoDesc.PrimitiveTopologyType			   = ToD3D12TopologyType(desc.topology);
	psoDesc.SampleMask						   = UINT_MAX;
	psoDesc.SampleDesc.Count				   = 1;

	D3D12Shader* vs = static_cast<D3D12Shader*>(desc.vertexShader);
	psoDesc.VS		= { vs->GetBytecode(), vs->GetBytecodeSize() };

	if (desc.pixelShader)
	{
		D3D12Shader* ps = static_cast<D3D12Shader*>(desc.pixelShader);
		psoDesc.PS		= { ps->GetBytecode(), ps->GetBytecodeSize() };
	}

	// Render target formats — 0 is valid (depth-only pass).
	psoDesc.NumRenderTargets = static_cast<UINT>(desc.renderTargetFormats.size());
	for (u32 i = 0; i < psoDesc.NumRenderTargets; ++i)
	{
		psoDesc.RTVFormats[i] = ToD3D12Format(desc.renderTargetFormats[i]);
	}

	if (desc.depthFormat != TextureFormat::Unknown)
	{
		psoDesc.DSVFormat = ToD3D12Format(desc.depthFormat);
	}

	ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pso)));
	LOG_DEBUG("D3D12Pipeline (graphics) created");
}

void D3D12Pipeline::Initialize(const PipelineDesc& /*desc*/)
{
}

void D3D12Pipeline::Cleanup()
{
	m_pso.Reset();
	m_rootSignature.Reset();
}

// ===========================================================================
// D3D12ComputePipeline
// ===========================================================================

// Default compute root signature:
//   Param 0 — Root CBV  (b0, space0): dispatch constants
//   Param 1 — Descriptor table: 8 UAVs (u0-u7, space0) — read/write resources
//   Param 2 — Descriptor table: 8 SRVs (t0-t7, space0) — read-only inputs

void D3D12ComputePipeline::BuildRootSignature(ID3D12Device* device)
{
	D3D12_DESCRIPTOR_RANGE ranges[2] = {};
	// UAVs: u0-u7
	ranges[0].RangeType							= D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	ranges[0].NumDescriptors					= 8;
	ranges[0].BaseShaderRegister				= 0;
	ranges[0].RegisterSpace						= 0;
	ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	// SRVs: t0-t7
	ranges[1].RangeType							= D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges[1].NumDescriptors					= 8;
	ranges[1].BaseShaderRegister				= 0;
	ranges[1].RegisterSpace						= 0;
	ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER params[3]		= {};
	params[0].ParameterType				= D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[0].Descriptor.ShaderRegister = 0; // b0
	params[0].ShaderVisibility			= D3D12_SHADER_VISIBILITY_ALL;

	params[1].ParameterType						  = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[1].DescriptorTable.NumDescriptorRanges = 1;
	params[1].DescriptorTable.pDescriptorRanges	  = &ranges[0]; // UAVs
	params[1].ShaderVisibility					  = D3D12_SHADER_VISIBILITY_ALL;

	params[2].ParameterType						  = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[2].DescriptorTable.NumDescriptorRanges = 1;
	params[2].DescriptorTable.pDescriptorRanges	  = &ranges[1]; // SRVs
	params[2].ShaderVisibility					  = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
	rootSigDesc.NumParameters			  = _countof(params);
	rootSigDesc.pParameters				  = params;
	rootSigDesc.Flags					  = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	ComRef<ID3DBlob> serialized, errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errorBlob);
	if (errorBlob)
	{
		LOG_WARNING("D3D12ComputePipeline root sig: {}", static_cast<char*>(errorBlob->GetBufferPointer()));
	}
	ThrowIfFailed(hr);
	ThrowIfFailed(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
											  IID_PPV_ARGS(&m_rootSignature)));
}

void D3D12ComputePipeline::InitializeWithDevice(ID3D12Device* device, const ComputePipelineDesc& desc)
{
	DYNAMIC_ASSERT(device, "D3D12ComputePipeline::InitializeWithDevice: device is null");
	DYNAMIC_ASSERT(desc.computeShader, "D3D12ComputePipeline::InitializeWithDevice: computeShader required");

	BuildRootSignature(device);

	D3D12Shader* cs = static_cast<D3D12Shader*>(desc.computeShader);

	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature					  = m_rootSignature.Get();
	psoDesc.CS								  = { cs->GetBytecode(), cs->GetBytecodeSize() };

	ThrowIfFailed(device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&m_pso)));
	LOG_DEBUG("D3D12ComputePipeline created");
}

void D3D12ComputePipeline::Initialize(const ComputePipelineDesc& /*desc*/)
{
}

void D3D12ComputePipeline::Cleanup()
{
	m_pso.Reset();
	m_rootSignature.Reset();
}

#endif
