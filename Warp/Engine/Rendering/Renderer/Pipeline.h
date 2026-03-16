#pragma once

#include <Common/CommonTypes.h>
#include <Rendering/Renderer/Texture.h>  // TextureFormat

class Shader;

// ---------------------------------------------------------------------------
// Input layout
// ---------------------------------------------------------------------------

struct InputElement
{
	// Use UINT32_MAX (or InputElement::AppendAligned) to let the driver
	// calculate the byte offset automatically from the previous element.
	static constexpr u32 AppendAligned = ~0u;

	String semanticName;              // "POSITION", "NORMAL", "TEXCOORD", etc.
	u32    semanticIndex   = 0;
	TextureFormat format;             // e.g. TextureFormat::RGB32F for Vec3
	u32    inputSlot       = 0;       // vertex buffer binding slot
	u32    alignedByteOffset = AppendAligned;
};

// ---------------------------------------------------------------------------
// Rasterizer state
// ---------------------------------------------------------------------------

struct RasterizerState
{
	enum class CullMode { None, Front, Back };
	enum class FillMode { Solid, Wireframe };

	CullMode cullMode = CullMode::Back;
	FillMode fillMode = FillMode::Solid;
	bool     depthClipEnable = true;
};

// ---------------------------------------------------------------------------
// Blend state
// ---------------------------------------------------------------------------

struct BlendState
{
	enum class BlendFactor
	{
		Zero, One,
		SrcColor,  InvSrcColor,
		SrcAlpha,  InvSrcAlpha,
		DestAlpha, InvDestAlpha,
		DestColor, InvDestColor,
	};

	enum class BlendOp { Add, Subtract, ReverseSubtract, Min, Max };

	BlendFactor srcBlend  = BlendFactor::SrcAlpha;
	BlendFactor destBlend = BlendFactor::InvSrcAlpha;
	BlendOp     blendOp   = BlendOp::Add;

	BlendFactor srcBlendAlpha  = BlendFactor::One;
	BlendFactor destBlendAlpha = BlendFactor::Zero;
	BlendOp     blendOpAlpha   = BlendOp::Add;
};

// ---------------------------------------------------------------------------
// Topology
// ---------------------------------------------------------------------------

enum class PrimitiveTopology
{
	PointList,
	LineList,
	LineStrip,
	TriangleList,
	TriangleStrip,
};

// ---------------------------------------------------------------------------
// Resource binding layout — describes how shaders access GPU resources.
//
// Each BindingSlot maps to one root parameter (D3D12) or one/many descriptor
// set bindings (Vulkan). The index of the slot in PipelineDesc::bindings is
// the rootIndex used by CommandList::SetConstantBufferView, SetShaderResources,
// and SetShaderResourceBuffer.
// ---------------------------------------------------------------------------

enum class BindingType
{
	ConstantBuffer,   // D3D12: root CBV.              Vulkan: uniform buffer.
	TextureTable,     // D3D12: SRV descriptor table.  Vulkan: combined image sampler array.
	StructuredBuffer, // D3D12: root SRV.              Vulkan: storage buffer.
};

struct BindingSlot
{
	BindingType type;
	u32 shaderRegister;  // HLSL register number (b0, t0, t4, etc.)
	u32 count;           // TextureTable: number of consecutive SRVs. Others: 1.
};

// ---------------------------------------------------------------------------
// Graphics pipeline descriptor
// ---------------------------------------------------------------------------

struct PipelineDesc
{
	Shader* vertexShader = nullptr;
	Shader* pixelShader  = nullptr;

	Vector<InputElement>   inputLayout;

	// Up to 8 simultaneous render targets.
	Vector<TextureFormat>  renderTargetFormats;
	TextureFormat          depthFormat = TextureFormat::Unknown;

	PrimitiveTopology      topology    = PrimitiveTopology::TriangleList;

	bool           enableDepthTest  = true;
	bool           enableDepthWrite = true;
	bool           enableStencilTest = false;
	bool           enableBlending   = false;

	BlendState     blend;
	RasterizerState rasterState;

	// Abstract binding layout — each entry becomes a root parameter (D3D12) or
	// descriptor set binding(s) (Vulkan). The array index is the rootIndex used
	// by CommandList binding methods.
	Vector<BindingSlot> bindings;
};

// ---------------------------------------------------------------------------
// Compute pipeline descriptor
// ---------------------------------------------------------------------------

struct ComputePipelineDesc
{
	Shader* computeShader = nullptr;
};

// ---------------------------------------------------------------------------
// Abstract pipeline state — graphics
// ---------------------------------------------------------------------------

class PipelineState
{
public:
	virtual ~PipelineState() = default;

	// Not used directly — created via Device::CreatePipelineState().
	virtual void  Initialize(const PipelineDesc& desc) = 0;
	virtual void  Cleanup()                            = 0;
	virtual void* GetNativeHandle() const              = 0;
};

// ---------------------------------------------------------------------------
// Abstract pipeline state — compute
//
// Kept separate from PipelineState so Renderer can dispatch compute without
// any platform casts. Device::CreateComputePipelineState returns this type.
// ---------------------------------------------------------------------------

class ComputePipelineState
{
public:
	virtual ~ComputePipelineState() = default;

	// Not used directly — created via Device::CreateComputePipelineState().
	virtual void  Initialize(const ComputePipelineDesc& desc) = 0;
	virtual void  Cleanup()                                   = 0;
	virtual void* GetNativeHandle() const                     = 0;
};
