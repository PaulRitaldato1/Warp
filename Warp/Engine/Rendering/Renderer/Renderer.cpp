#include <Rendering/Renderer/Renderer.h>
#include <Debugging/Assert.h>

void Renderer::BeginFrame()
{
	// Wait for the GPU to finish with the oldest in-flight frame before reusing its slot.
	const FrameSyncPoint& retiring = m_frameSyncPoints[m_frameIndex];
	if (retiring.graphicsFenceValue > 0)
	{
		m_graphicsQueue->WaitForValue(retiring.graphicsFenceValue);
	}
	if (retiring.computeFenceValue > 0)
	{
		m_computeQueue->WaitForValue(retiring.computeFenceValue);
	}

	// Retire the oldest frame's upload-buffer slice so the ring buffer can reuse it.
	m_uploadBuffer->OnBeginFrame();

	// Reset all CPU-side per-frame allocations.
	m_frameArena.Reset();

	// Open all command lists for this frame slot.
	for (auto& list : m_graphicsLists) { list->Begin(m_frameIndex); }
	for (auto& list : m_computeLists)  { list->Begin(m_frameIndex); }
	m_copyList->Begin(m_frameIndex);
}

void Renderer::Draw()
{
	switch (m_renderPath)
	{
		case RenderPath::Deferred:    DrawDeferred();    break;
		case RenderPath::ForwardPlus: DrawForwardPlus(); break;
	}
}

void Renderer::EndFrame()
{
	// Close all command lists.
	for (auto& list : m_graphicsLists) { list->End(); }
	for (auto& list : m_computeLists)  { list->End(); }
	m_copyList->End();

	// Copy queue first — graphics/compute may read data uploaded this frame.
	m_copyQueue->Submit(*m_copyList);

	// Submit worker lists and track the last signaled fence value per queue.
	FrameSyncPoint& sync = m_frameSyncPoints[m_frameIndex];
	for (auto& list : m_graphicsLists)
	{
		sync.graphicsFenceValue = m_graphicsQueue->Submit(*list);
	}
	for (auto& list : m_computeLists)
	{
		sync.computeFenceValue = m_computeQueue->Submit(*list);
	}

	m_swapChain->Present();

	m_frameIndex = (m_frameIndex + 1) % k_framesInFlight;
}

void Renderer::DrawDeferred()
{
	// TODO: G-buffer pass, lighting pass, post-process
}

void Renderer::DrawForwardPlus()
{
	// TODO: light culling compute pass, forward pass
}
