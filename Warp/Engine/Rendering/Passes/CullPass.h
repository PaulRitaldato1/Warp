#pragma once

#include <Common/CommonTypes.h>
#include <Math/Math.h>
#include <Rendering/Passes/PassContext.h>
#include <Rendering/Renderer/PipelineLibrary.h>

class Buffer;
class RenderGraph;

// GPU frustum culling. One dispatch per view, each counting the survivors into
// the view's draw args and writing their slots into its visible index list.

ComputePipelineHandle CreateCullPassPipeline(PipelineLibrary& library);

struct CullView
{
	Array<Vec4, 6> frustum = {};
	u32 requiredFlags	   = 0; // RenderFlags an instance needs, e.g. CastShadow for a shadow view

	Buffer* drawArgs	   = nullptr; // left in IndirectArgument for the draw
	Buffer* visibleIndices = nullptr; // left in ShaderResource for the draw
};

struct CullPassInputs
{
	ComputePipelineHandle pipeline;

	// Both left in ShaderResource by the scene upload.
	Buffer* instances	 = nullptr;
	Buffer* regionStarts = nullptr;
	u32 slotCount		 = 0;

	Vector<CullView> views;
};

void AddCullPass(RenderGraph& graph, const PassContext& context, CullPassInputs inputs);
