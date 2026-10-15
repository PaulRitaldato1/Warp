#include <Rendering/Passes/ImGuiPass.h>
#include <Rendering/Renderer/CommandList.h>
#include <Rendering/Renderer/RenderGraph.h>
#include <Rendering/Renderer/SwapChain.h>
#include <UI/ImGuiBackend.h>

void ImGuiPass::AddToGraph(RenderGraph& graph, FrameBlackboard& blackboard, const FrameView& view,
						   const PassContext& context)
{
	if (!context.imgui)
	{
		return;
	}

	graph.AddPass("ImGui").Execute(
		[context](CommandList& cmd)
		{
			// Binds the back buffer itself rather than relying on tonemap leaving it
			// bound. Vulkan loads the attachment, so the image survives the rebind.
			const DescriptorHandle backBufferRTV = context.swapChain->GetCurrentRTV();
			cmd.SetRenderTargets(1, &backBufferRTV, nullptr);
			context.imgui->Render(&cmd);
		});
}
