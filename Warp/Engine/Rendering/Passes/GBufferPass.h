#pragma once

#include <Common/CommonTypes.h>
#include <Math/Math.h>
#include <Rendering/Passes/PassContext.h>
#include <Rendering/Renderer/DrawList.h>
#include <Rendering/Renderer/PipelineLibrary.h>

class Buffer;
class Texture;
class RenderGraph;

// Draws every visible batch into the GBuffer and the depth buffer.

// The renderer owns them and recreates them on resize.
struct GBufferTargets
{
	Texture* albedo	  = nullptr; // RGBA8
	Texture* normal	  = nullptr; // RGBA16F
	Texture* material = nullptr; // RGBA8
	Texture* emissive = nullptr; // RGBA8
};

PipelineHandle CreateGBufferPassPipeline(PipelineLibrary& library);

struct GBufferPassInputs
{
	PipelineHandle pipeline;

	GBufferTargets targets;	  // left in RenderTarget
	Texture* depth = nullptr; // left in DepthWrite

	Mat4 viewProj = {};

	// The renderer's draw list, which outlives the frame's graph.
	Span<const BatchItem> batches;
	Buffer* instances	   = nullptr;
	Buffer* visibleIndices = nullptr;
	Buffer* drawArgs	   = nullptr; // in IndirectArgument
};

void AddGBufferPass(RenderGraph& graph, const PassContext& context, const GBufferPassInputs& inputs);
