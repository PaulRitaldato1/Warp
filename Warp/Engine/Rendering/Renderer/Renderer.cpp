#include <Rendering/Renderer/Renderer.h>
#include <Debugging/Assert.h>
#include <Debugging/Logging.h>

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
	CommandList& cmd = *m_graphicsLists[0];

	// Transition the current back buffer from present state to renderable.
	m_swapChain->TransitionToRenderTarget(cmd);

	// Bind and clear the back buffer + depth.
	DescriptorHandle rtv = m_swapChain->GetCurrentRTV();
	cmd.SetRenderTargets(1, &rtv, m_depthHandle);
	cmd.ClearRenderTarget(rtv, 0.1f, 0.1f, 0.1f, 1.f);
	if (m_depthHandle.IsValid())
		cmd.ClearDepthStencil(m_depthHandle, 1.f, 0);

	// Full-screen viewport and scissor.
	const f32 w = static_cast<f32>(m_swapChain->GetWidth());
	const f32 h = static_cast<f32>(m_swapChain->GetHeight());
	cmd.SetViewport(0.f, 0.f, w, h);
	cmd.SetScissorRect(0, 0, m_swapChain->GetWidth(), m_swapChain->GetHeight());

	// Test triangle — drawn until a scene system replaces it.
	if (m_testTriPSO)
	{
		cmd.SetPipelineState(m_testTriPSO.get());
		cmd.SetPrimitiveTopology(PrimitiveTopology::TriangleList);
		cmd.Draw(3);
	}

	// TODO: G-buffer pass, lighting pass, post-process

	// Transition back buffer to present state before EndFrame submits the list.
	m_swapChain->TransitionToPresent(cmd);
}

void Renderer::DrawForwardPlus()
{
	// TODO: light culling compute pass, forward opaque pass
	// Shares the same back-buffer setup as deferred for now.
	DrawDeferred();
}

void Renderer::CreateTestTriangle()
{
	ShaderDesc vsDesc;
	vsDesc.type       = ShaderType::Vertex;
	vsDesc.entryPoint = "VSMain";
	vsDesc.sourceCode = k_triVSSrc;
	m_testTriVS = m_device->CreateShader(vsDesc);

	ShaderDesc psDesc;
	psDesc.type       = ShaderType::Pixel;
	psDesc.entryPoint = "PSMain";
	psDesc.sourceCode = k_triPSSrc;
	m_testTriPS = m_device->CreateShader(psDesc);

	PipelineDesc triDesc;
	triDesc.vertexShader          = m_testTriVS.get();
	triDesc.pixelShader           = m_testTriPS.get();
	triDesc.inputLayout           = {};
	triDesc.renderTargetFormats   = { TextureFormat::BGRA8 };
	triDesc.depthFormat           = TextureFormat::Depth32F;
	triDesc.topology              = PrimitiveTopology::TriangleList;
	triDesc.enableDepthTest       = false;
	triDesc.enableDepthWrite      = false;
	triDesc.enableStencilTest     = false;
	triDesc.enableBlending        = false;
	triDesc.rasterState.cullMode  = RasterizerState::CullMode::None;
	triDesc.rasterState.fillMode  = RasterizerState::FillMode::Solid;
	m_testTriPSO = m_device->CreatePipelineState(triDesc);

	LOG_DEBUG("Renderer: test triangle PSO ready");
}
