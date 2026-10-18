#pragma once

#include <Common/CommonTypes.h>

#include <functional>

class CommandList;
class RenderGraph;

using RenderPassExecute = std::function<void(CommandList&)>;

// Returned by RenderGraph::AddPass to finish describing the pass:
//   graph.AddPass("Tonemap").Execute([&](CommandList& cmd) { ... });
// Read() and Write() go here later, which is what automatic barriers need.
class RenderPassBuilder
{
public:
	RenderPassBuilder& Execute(RenderPassExecute execute);

private:
	friend class RenderGraph;

	RenderPassBuilder(RenderGraph& graph, u32 passIndex) : m_graph(graph), m_passIndex(passIndex)
	{
	}

	// An index rather than a pointer, since adding another pass can grow the vector.
	RenderGraph& m_graph;
	u32 m_passIndex;
};

// The frame's passes, rebuilt every frame and recorded in the order they were
// added. v1 tracks no resources, so each pass does its own transitions.
class RenderGraph
{
public:
	// The profiler keeps the name pointer past the frame, so it must be a string literal.
	RenderPassBuilder AddPass(const char* name);

	// Records every pass into cmd, each inside a GPU marker and a profile scope,
	// then clears them, so no lambda outlives the frame it was added in.
	void Execute(CommandList& cmd);

	struct PassStats
	{
		const char* name = nullptr;
		f64 recordMillis = 0.0; // CPU time to record it, not GPU time
	};

	// The last executed frame. The UI is built before this frame's passes are
	// added, so this is what the pass list window shows.
	const Vector<PassStats>& GetLastFrameStats() const
	{
		return m_lastFrameStats;
	}

private:
	friend class RenderPassBuilder;

	struct Pass
	{
		const char* name = nullptr;
		RenderPassExecute execute;
	};

	Vector<Pass> m_passes;
	Vector<PassStats> m_lastFrameStats;
};
