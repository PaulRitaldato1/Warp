#include <Debugging/Assert.h>
#include <Debugging/GPUMarker.h>
#include <Debugging/Profiler.h>
#include <Rendering/Renderer/RenderGraph.h>

RenderPassBuilder& RenderPassBuilder::Execute(RenderPassExecute execute)
{
	m_graph.m_passes[m_passIndex].execute = std::move(execute);
	return *this;
}

RenderPassBuilder RenderGraph::AddPass(const char* name)
{
	DYNAMIC_ASSERT(name, "RenderGraph::AddPass: name is null");

	const u32 index = static_cast<u32>(m_passes.size());
	m_passes.push_back({ name, {} });
	return RenderPassBuilder(*this, index);
}

void RenderGraph::Execute(CommandList& cmd)
{
	m_lastFrameStats.clear();

	for (Pass& pass : m_passes)
	{
		DYNAMIC_ASSERT(pass.execute, "RenderGraph: a pass was added without Execute");

		const int64 startMicros = Profiler::NowMicros();
		{
			PROFILE_SCOPE(pass.name);
			WARP_SCOPED_GPU_MARKER(&cmd, pass.name);

			if (pass.execute)
			{
				pass.execute(cmd);
			}
		}

		const f64 recordMillis = static_cast<f64>(Profiler::NowMicros() - startMicros) / 1000.0;
		m_lastFrameStats.push_back({ pass.name, recordMillis });
	}

	m_passes.clear();
}
