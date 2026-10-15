#pragma once

#include <Rendering/Passes/RenderPass.h>

// Draws the editor UI over the tonemapped back buffer. The ImGui backend owns its
// own pipeline, so there is nothing to create at init. Adds nothing while ImGui
// is not initialized.
class ImGuiPass : public RenderPass
{
public:
	void AddToGraph(RenderGraph& graph, FrameBlackboard& blackboard, const FrameView& view,
					const PassContext& context) override;
};
