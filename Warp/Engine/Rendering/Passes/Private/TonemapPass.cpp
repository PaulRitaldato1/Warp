#include <Core/Console/ConsoleVariable.h>
#include <Rendering/Passes/TonemapPass.h>
#include <Rendering/Renderer/RenderGraph.h>
#include <Rendering/Renderer/SwapChain.h>
#include <Rendering/Renderer/Texture.h>

static Cvar<float> CvarExposureEV("r.Tonemap.ExposureEV", 0.0f, "Adjusts the exposure that the tonemapper uses");
static Cvar<int> CvarTonemapperMethod("r.Tonemap.Method", 1, "0 clamp, 1 Reinhard, 2 ACES Narkowicz");
static Cvar<f32> CvarWhitePoint("r.Tonemap.WhitePoint", 4.0f,
								"Extended Reinhard: brightness after exposure that maps to white");

// Matches TonemapConstants in Tonemap.hlsl.
struct TonemapConstants
{
	float exposureEV;
	float whitePoint;
	int32 tonemapper;
	u32 padding;
};

PipelineHandle CreateTonemapPassPipeline(PipelineLibrary& library)
{
	// Writes the swap chain.
	return library.Create(MakeFullscreenPipeline(
		"Tonemap", "Shaders/Tonemap.hlsl", TextureFormat::BGRA8,
		{
			{ BindingType::ConstantBuffer, 0, 1 }, // rootIndex 0: b0 — exposureEV, tonemapper, padding
			{ BindingType::TextureTable, 0, 1 },   // rootIndex 1: t0 - HDR target
		}));
}

void AddTonemapPass(RenderGraph& graph, const PassContext& context, const TonemapPassInputs& inputs)
{
	TonemapConstants constants = {};
	constants.exposureEV	   = CvarExposureEV.Get();
	constants.tonemapper	   = CvarTonemapperMethod.Get();
	constants.whitePoint	   = CvarWhitePoint.Get();

	graph.AddPass("Tonemap").Execute(
		[context, inputs, constants](CommandList& cmd)
		{
			cmd.SetPipelineState(context.pipelines->Get(inputs.pipeline));
			cmd.SetPrimitiveTopology(PrimitiveTopology::TriangleList);

			cmd.TransitionTexture(inputs.hdrTarget, ResourceState::ShaderResource);
			inputs.swapChain->TransitionToRenderTarget(cmd);
			const DescriptorHandle backBufferRTV = inputs.swapChain->GetCurrentRTV();
			cmd.SetRenderTargets(1, &backBufferRTV, nullptr);

			BindConstants(cmd, context, 0, constants);
			cmd.SetShaderResources(1, { inputs.hdrTarget });

			cmd.Draw(3);
		});
}
