#pragma once

#include <Common/CommonTypes.h>
#include <Rendering/Passes/PassContext.h>

class FrameBlackboard;
class RenderGraph;
struct FrameView;

// A pass owns its pipelines and any GPU resources that live across frames. The
// renderer only decides the order: DrawDeferred calls AddToGraph on each pass.
class RenderPass
{
public:
	virtual ~RenderPass() = default;

	// Once, after the device exists. Create pipelines and persistent resources here.
	virtual void Init(const PassInitContext& context)
	{
	}

	// After the swap chain resized, with the GPU idle. Recreate size dependent
	// resources here.
	virtual void OnResize(u32 width, u32 height)
	{
	}

	// Every frame. This body is setup: read the view and the blackboard, add the
	// pass's outputs to the blackboard, build constants. The lambda given to
	// RenderGraph's Execute only records.
	virtual void AddToGraph(RenderGraph& graph, FrameBlackboard& blackboard, const FrameView& view,
							const PassContext& context) = 0;
};
