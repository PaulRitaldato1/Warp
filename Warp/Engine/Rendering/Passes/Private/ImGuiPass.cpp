#include <Rendering/Passes/ImGuiPass.h>
#include <Rendering/Renderer/CommandList.h>
#include <Rendering/Renderer/RenderGraph.h>
#include <Rendering/Renderer/SwapChain.h>
#include <UI/ImGuiBackend.h>

void AddImGuiPass(RenderGraph& graph, ImGuiBackend* backend, SwapChain* swapChain)
{
	graph.AddPass("ImGui").Execute(
		[backend, swapChain](CommandList& cmd)
		{
			// Binds the back buffer itself rather than relying on tonemap leaving it
			// bound. Vulkan loads the attachment, so the image survives the rebind.
			const DescriptorHandle backBufferRTV = swapChain->GetCurrentRTV();
			cmd.SetRenderTargets(1, &backBufferRTV, nullptr);
			backend->Render(&cmd);
		});
}
