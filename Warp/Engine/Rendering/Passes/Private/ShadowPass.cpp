#include <Rendering/Passes/ShadowPass.h>
#include <Rendering/Renderer/RenderGraph.h>
#include <Rendering/Renderer/Texture.h>

// Matches PerView in DirectionalShadow.hlsl. Bound once for the pass.
struct ShadowViewConstants
{
	Mat4 lightViewProj;
};

// Matches ShadowDrawConstants in DirectionalShadow.hlsl. One per batch.
struct ShadowDrawConstants
{
	uint instanceOffset;
	u32 padding[3];
};

PipelineHandle CreateShadowPassPipeline(PipelineLibrary& library)
{
	// Depth only, so no pixel shader.
	GraphicsPipelineInfo info;
	info.name		  = "Directional Shadow";
	info.vertexShader = ShaderDesc::FromFile(ShaderType::Vertex, "Shaders/DirectionalShadow.hlsl", "VSMain");

	PipelineDesc& desc = info.state;
	// Position only. The shadow VS reads nothing else, and binding this stream
	// alone is what makes the split pay off.
	desc.inputLayout = {
		{ "POSITION", 0, TextureFormat::RGB32F, 0, InputElement::AppendAligned },
	};
	desc.renderTargetFormats = {};
	desc.depthFormat		 = TextureFormat::Depth32F;
	desc.topology			 = PrimitiveTopology::TriangleList;
	desc.enableDepthTest	 = true;
	desc.enableDepthWrite	 = true;
	desc.enableStencilTest	 = false;
	desc.enableBlending		 = false;

	// Same depth values as CullMode::None here, at roughly half the rasterization.
	desc.rasterState.cullMode = RasterizerState::CullMode::Back;
	desc.rasterState.fillMode = RasterizerState::FillMode::Solid;

	// Starting values. Tune against the scene.
	desc.rasterState.depthBias			  = 100;
	desc.rasterState.slopeScaledDepthBias = 2.f;
	desc.rasterState.depthBiasClamp		  = 0.f;

	desc.bindings = {
		{ BindingType::StructuredBuffer, 0, 1 }, // rootIndex 0: t0 — structured buffer with instance data
		{ BindingType::ConstantBuffer, 0, 1 },	 // rootIndex 1: b0 - shadow constants
		{ BindingType::ConstantBuffer, 1, 1 },	 // rootIndex 2: b1 — per-view lightViewProj
		{ BindingType::StructuredBuffer, 1, 1 }, // rootIndex 3: t1 — slot indices
	};

	return library.Create(info);
}

void AddShadowPass(RenderGraph& graph, const PassContext& context, const ShadowPassInputs& inputs)
{
	graph.AddPass("Shadow").Execute(
		[context, inputs](CommandList& cmd)
		{
			cmd.TransitionTexture(inputs.shadowMap, ResourceState::DepthWrite);
			cmd.SetRenderTargets(0, nullptr, inputs.shadowMap);
			cmd.ClearDepthStencil(inputs.shadowMap, 1.f, 0);

			cmd.SetViewport(0.f, 0.f, static_cast<f32>(inputs.resolution), static_cast<f32>(inputs.resolution));
			cmd.SetScissorRect(0, 0, inputs.resolution, inputs.resolution);

			cmd.SetPipelineState(context.pipelines->Get(inputs.pipeline));
			cmd.SetPrimitiveTopology(PrimitiveTopology::TriangleList);

			if (inputs.bHasLight)
			{
				BindConstants(cmd, context, 2, ShadowViewConstants{ inputs.lightViewProj });

				if (!inputs.batches.empty())
				{
					cmd.SetShaderResourceBuffer(0, inputs.instances, 0);
					cmd.SetShaderResourceBuffer(3, inputs.visibleIndices, 0);
				}

				for (const BatchItem& batch : inputs.batches)
				{
					ShadowDrawConstants drawConstants = {};
					drawConstants.instanceOffset	  = batch.instanceOffset;
					BindConstants(cmd, context, 1, drawConstants);

					// Depth only: position is the sole stream this pass reads.
					cmd.SetVertexBuffer(batch.positionBuffer);
					cmd.SetIndexBuffer(batch.indexBuffer);

					cmd.DrawIndexedIndirect(inputs.drawArgs, batch.batchIndex * sizeof(DrawIndexedArgs));
				}
			}

			// Transition shadow map for later use in the lighting pass.
			cmd.TransitionTexture(inputs.shadowMap, ResourceState::ShaderResource);
		});
}
