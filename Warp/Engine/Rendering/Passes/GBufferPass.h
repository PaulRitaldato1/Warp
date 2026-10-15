#pragma once

#include <Common/CommonTypes.h>
#include <Rendering/Passes/RenderPass.h>
#include <Rendering/Renderer/PipelineLibrary.h>

class Texture;

// Draws every visible batch into the GBuffer targets and the depth buffer.

// Added to the blackboard for lighting. All left in RenderTarget, depth in DepthWrite.
struct GBufferOutput
{
	Texture* albedo	  = nullptr; // RGBA8
	Texture* normal	  = nullptr; // RGBA16F
	Texture* material = nullptr; // RGBA8
	Texture* emissive = nullptr; // RGBA8
	Texture* depth	  = nullptr; // Depth32F
};

class GBufferPass : public RenderPass
{
public:
	~GBufferPass() override;

	void Init(const PassInitContext& context) override;
	void OnResize(u32 width, u32 height) override;
	void AddToGraph(RenderGraph& graph, FrameBlackboard& blackboard, const FrameView& view,
					const PassContext& context) override;

private:
	void CreateTargets(u32 width, u32 height);

	Device* m_device = nullptr;
	PipelineHandle m_pipeline;

	URef<Texture> m_albedo;
	URef<Texture> m_normal;
	URef<Texture> m_material;
	URef<Texture> m_emissive;
	URef<Texture> m_depth;
};
