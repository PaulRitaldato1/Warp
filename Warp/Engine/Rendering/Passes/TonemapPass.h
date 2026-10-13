#pragma once

#include <Common/CommonTypes.h>
#include <Rendering/Passes/PassContext.h>
#include <Rendering/Renderer/PipelineLibrary.h>

class SwapChain;
class Texture;
class RenderGraph;

// Exposure, then the r.Tonemap.Method curve, then the sRGB encode, from the HDR
// target into the back buffer. The r.Tonemap cvars live in TonemapPass.cpp.

PipelineHandle CreateTonemapPassPipeline(PipelineLibrary& library);

struct TonemapPassInputs
{
	PipelineHandle pipeline;

	Texture* hdrTarget	 = nullptr; // moved to ShaderResource here
	SwapChain* swapChain = nullptr; // the back buffer is left in RenderTarget
};

void AddTonemapPass(RenderGraph& graph, const PassContext& context, const TonemapPassInputs& inputs);
