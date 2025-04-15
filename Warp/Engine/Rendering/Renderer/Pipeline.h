#pragma once

#include <Common/CommonTypes.h>

class Shader;

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
	Shader* VertexShader;
	Shader* FragmantShader;
	Shader* ComputeShader;

	bool bEnableDepthTest;
	bool bEnableStencilTest;

	bool bEnableBlending;
	BlendState Blend;

	RasterizerState RasterState;
};

class PipelineState
{
public:
	virtual ~PipelineState() = default;

	virtual void Initialize(const PipelineDesc& Desc) = 0;

	virtual void Bind() const = 0;

	virtual void Cleanup() = 0;
};