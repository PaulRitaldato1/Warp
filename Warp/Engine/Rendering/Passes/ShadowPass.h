#pragma once

#include <Common/CommonTypes.h>
#include <Rendering/Passes/RenderPass.h>
#include <Rendering/Renderer/PipelineLibrary.h>

class Texture;

// Depth from the sun's point of view, for the lighting pass to sample.

// Added to the blackboard for lighting. Left in ShaderResource. Cleared even
// without a sun, so lighting can always sample it.
struct ShadowOutput
{
	Texture* sunShadowMap = nullptr;
};

class ShadowPass : public RenderPass
{
public:
	~ShadowPass() override;

	void Init(const PassInitContext& context) override;
	void AddToGraph(RenderGraph& graph, FrameBlackboard& blackboard, const FrameView& view,
					const PassContext& context) override;

private:
	static constexpr u32 k_resolution = 4096;

	PipelineHandle m_pipeline;
	URef<Texture> m_sunShadowMap;
};
