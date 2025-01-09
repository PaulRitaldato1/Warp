#pragma once

#include <Common/CommonTypes.h>

class IShader;

struct BlendState
{
	bool bEnableAlphaBlend = false;

	enum class BlendFactor
	{
		Zero,
		One,
		SrcColor,
		InvSrcColor,
		SrcAlpha,
		InvSrcAlpha,
		DestAlpha,
		InvDestAlpha,
		DestColor,
		InvDestColor,
	};

	enum class BlendOperation
	{
		Add,
		Subtract,
		ReverseSubtract,
		Min,
		Max,
	};

	BlendFactor SrcBlend;
	BlendFactor DestBlend;
	BlendOperation BlendOp;
};

struct RasterizerState
{
	bool bCullFace = true;

	enum class FaceCullMode
	{
		None,
		Front,
		Back,
	};

	enum class FillMode
	{
		Solid,
		Wireframe,
	};

	FaceCullMode cullMode = FaceCullMode::Back;
	FillMode fillMode	  = FillMode::Solid;
};

struct PipelineDesc
{
	IShader* VertexShader;
	IShader* FragmantShader;
	IShader* ComputeShader;

	bool bEnableDepthTest;
	bool bEnableStencilTest;

	bool bEnableBlending;
	BlendState Blend;

	RasterizerState RasterState;
};

class IPipelineState
{
public:
	virtual ~IPipelineState() = default;

	virtual void Initialize(const PipelineDesc& Desc) = 0;

	virtual void Bind() const = 0;

	virtual void Cleanup() = 0;
};