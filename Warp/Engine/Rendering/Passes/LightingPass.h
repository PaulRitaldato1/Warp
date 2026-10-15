#pragma once

#include <Common/CommonTypes.h>
#include <Rendering/Passes/RenderPass.h>
#include <Rendering/Renderer/PipelineLibrary.h>

class Texture;

// Full screen deferred lighting. Reads the GBuffer, depth and sun shadow map, and
// writes linear HDR.

// Added to the blackboard for tonemap and later post passes. Left in RenderTarget.
struct SceneColorOutput
{
	Texture* hdr = nullptr; // RGBA16F, linear
};

class LightingPass : public RenderPass
{
public:
	~LightingPass() override;

	void Init(const PassInitContext& context) override;
	void OnResize(u32 width, u32 height) override;
	void AddToGraph(RenderGraph& graph, FrameBlackboard& blackboard, const FrameView& view,
					const PassContext& context) override;

private:
	void CreateTarget(u32 width, u32 height);

	Device* m_device = nullptr;
	PipelineHandle m_pipeline;
	URef<Texture> m_hdr;
};
