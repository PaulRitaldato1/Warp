#pragma once

#include <Common/CommonTypes.h>
#include <Rendering/Passes/RenderPass.h>
#include <Rendering/Renderer/PipelineLibrary.h>

// Exposure, then the r.Tonemap.Method curve, then the sRGB encode, from the scene
// color into the back buffer, which it leaves in RenderTarget. The r.Tonemap
// cvars live in TonemapPass.cpp.
class TonemapPass : public RenderPass
{
public:
	void Init(const PassInitContext& context) override;
	void AddToGraph(RenderGraph& graph, FrameBlackboard& blackboard, const FrameView& view,
					const PassContext& context) override;

private:
	PipelineHandle m_pipeline;
};
