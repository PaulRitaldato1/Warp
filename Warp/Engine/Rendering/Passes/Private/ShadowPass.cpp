#include <Rendering/Passes/ShadowPass.h>
#include <Rendering/Renderer/Device.h>
#include <Rendering/Renderer/FrameBlackboard.h>
#include <Rendering/Renderer/FrameView.h>
#include <Rendering/Renderer/GPUScene.h>
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

static PipelineHandle CreatePipeline(PipelineLibrary& library)
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

ShadowPass::~ShadowPass() = default;

void ShadowPass::Init(const PassInitContext& context)
{
	m_pipeline = CreatePipeline(*context.pipelines);

	// Size independent, so made once and kept across resizes.
	TextureDesc desc;
	desc.width	   = k_resolution;
	desc.height	   = k_resolution;
	desc.format	   = TextureFormat::Depth32F;
	desc.usage	   = TextureUsage::DepthStencilSampled;
	m_sunShadowMap = context.device->CreateTexture(desc);
}

void ShadowPass::AddToGraph(RenderGraph& graph, FrameBlackboard& blackboard, const FrameView& view,
							const PassContext& context)
{
	const SceneDrawView casters = blackboard.Get<GPUSceneOutput>().sunShadow;

	blackboard.Add(ShadowOutput{ m_sunShadowMap.get() });

	graph.AddPass("Shadow").Execute(
		[context, pipeline = m_pipeline, shadowMap = m_sunShadowMap.get(), casters, bHasSun = view.sky.bHasSun,
		 sunViewProj = view.sky.sunViewProj](CommandList& cmd)
		{
			cmd.TransitionTexture(shadowMap, ResourceState::DepthWrite);
			cmd.SetRenderTargets(0, nullptr, shadowMap);
			cmd.ClearDepthStencil(shadowMap, 1.f, 0);

			cmd.SetViewport(0.f, 0.f, static_cast<f32>(k_resolution), static_cast<f32>(k_resolution));
			cmd.SetScissorRect(0, 0, k_resolution, k_resolution);

			cmd.SetPipelineState(context.pipelines->Get(pipeline));
			cmd.SetPrimitiveTopology(PrimitiveTopology::TriangleList);

			if (bHasSun)
			{
				BindConstants(cmd, context, 2, ShadowViewConstants{ sunViewProj });

				if (!casters.batches.empty())
				{
					cmd.SetShaderResourceBuffer(0, casters.instances, 0);
					cmd.SetShaderResourceBuffer(3, casters.visibleIndices, 0);
				}

				for (const BatchItem& batch : casters.batches)
				{
					ShadowDrawConstants drawConstants = {};
					drawConstants.instanceOffset	  = batch.instanceOffset;
					BindConstants(cmd, context, 1, drawConstants);

					// Depth only: position is the sole stream this pass reads.
					cmd.SetVertexBuffer(batch.positionBuffer);
					cmd.SetIndexBuffer(batch.indexBuffer);

					cmd.DrawIndexedIndirect(casters.drawArgs, batch.batchIndex * sizeof(DrawIndexedArgs));
				}
			}

			// Transition shadow map for later use in the lighting pass.
			cmd.TransitionTexture(shadowMap, ResourceState::ShaderResource);
		});
}
