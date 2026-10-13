#pragma once

#include <Common/CommonTypes.h>
#include <Math/Math.h>
#include <Rendering/Passes/PassContext.h>
#include <Rendering/Renderer/DrawList.h>
#include <Rendering/Renderer/PipelineLibrary.h>

class Buffer;
class Texture;
class RenderGraph;

// Depth from the directional light's point of view, for the lighting pass to sample.

PipelineHandle CreateShadowPassPipeline(PipelineLibrary& library);

struct ShadowPassInputs
{
	PipelineHandle pipeline;

	Texture* shadowMap = nullptr; // left in ShaderResource for lighting
	u32 resolution	   = 0;

	// Without a light the map is still cleared, so lighting samples it as unshadowed.
	bool bHasLight	   = false;
	Mat4 lightViewProj = {};

	// The renderer's draw list, which outlives the frame's graph.
	Span<const BatchItem> batches;
	Buffer* instances	   = nullptr;
	Buffer* visibleIndices = nullptr;
	Buffer* drawArgs	   = nullptr; // in IndirectArgument
};

void AddShadowPass(RenderGraph& graph, const PassContext& context, const ShadowPassInputs& inputs);
