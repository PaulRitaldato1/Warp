#include <Rendering/Lighting/LightData.h>
#include <Rendering/Passes/GBufferPass.h>
#include <Rendering/Passes/LightingPass.h>
#include <Rendering/Passes/ShadowPass.h>
#include <Rendering/Renderer/Device.h>
#include <Rendering/Renderer/FrameBlackboard.h>
#include <Rendering/Renderer/FrameView.h>
#include <Rendering/Renderer/RenderGraph.h>
#include <Rendering/Renderer/Texture.h>

static PipelineHandle CreatePipeline(PipelineLibrary& library)
{
	GraphicsPipelineInfo info = MakeFullscreenPipeline(
		"Deferred Lighting", "Shaders/DeferredLighting.hlsl", TextureFormat::RGBA16F,
		{
			{ BindingType::ConstantBuffer, 0, 1 }, // rootIndex 0: b0 — lighting constants
			{ BindingType::TextureTable, 0,
			  5 }, // rootIndex 1: t0-t4 — GBuffer textures (albedo, normal, material, depth, emissive)
			{ BindingType::StructuredBuffer, 5, 1 }, // rootIndex 2: t5 — Light Infos buffer
			{ BindingType::TextureTable, 6, 1 },	 // rootIndex 3: t6 — Shadow map
		});

	// Its own VS, which also projects each corner onto the far plane for the view ray.
	info.vertexShader.filePath = "Shaders/DeferredLighting.hlsl";

	info.state.samplers = {
		{ 0, SamplerFilter::Point, SamplerAddressMode::Clamp },				// s0 — GBuffer sampling
		{ 2, SamplerFilter::ComparisonLinear, SamplerAddressMode::Border }, // s2 — Shadow map comparison
	};

	return library.Create(info);
}

// The scene lights in the GPU layout, then the sun.
static Vector<LightInfo> BuildLightInfos(const FrameView& view)
{
	Vector<LightInfo> lightInfos;
	lightInfos.reserve(view.lights.items.size() + 1);

	for (const LightItem& light : view.lights.items)
	{
		LightInfo info;
		info.position		= light.position;
		info.range			= light.range;
		info.color			= light.color;
		info.intensity		= light.intensity;
		info.direction		= light.direction;
		info.type			= static_cast<int32>(light.type);
		info.innerConeAngle = light.innerConeAngle;
		info.outerConeAngle = light.outerConeAngle;
		lightInfos.push_back(info);
	}

	if (view.sky.bHasSun)
	{
		lightInfos.push_back(view.sky.sun);
	}

	return lightInfos;
}

LightingPass::~LightingPass() = default;

void LightingPass::Init(const PassInitContext& context)
{
	m_device   = context.device;
	m_pipeline = CreatePipeline(*context.pipelines);
	CreateTarget(context.width, context.height);
}

void LightingPass::OnResize(u32 width, u32 height)
{
	CreateTarget(width, height);
}

void LightingPass::CreateTarget(u32 width, u32 height)
{
	TextureDesc desc;
	desc.width	= width;
	desc.height = height;
	desc.format = TextureFormat::RGBA16F;
	desc.usage	= TextureUsage::RenderTarget;
	m_hdr		= m_device->CreateTexture(desc);
}

void LightingPass::AddToGraph(RenderGraph& graph, FrameBlackboard& blackboard, const FrameView& view,
							  const PassContext& context)
{
	const GBufferOutput gbuffer = blackboard.Get<GBufferOutput>();
	Texture* shadowMap			= blackboard.Get<ShadowOutput>().sunShadowMap;

	blackboard.Add(SceneColorOutput{ m_hdr.get() });

	Vector<LightInfo> lightInfos = BuildLightInfos(view);

	LightPassConstants constants = {};
	constants.invViewProj		 = view.camera.invViewProj;
	constants.cameraPosition	 = view.camera.position;
	constants.lightCount		 = static_cast<int32>(lightInfos.size());
	constants.sky				 = view.sky.parameters;
	constants.lightViewProj		 = view.sky.sunViewProj;
	constants.shadowBias		 = 0.005f;

	graph.AddPass("Lighting")
		.Execute(
			[context, pipeline = m_pipeline, hdr = m_hdr.get(), gbuffer, shadowMap, constants,
			 lightInfos = std::move(lightInfos)](CommandList& cmd)
			{
				cmd.TransitionTexture(gbuffer.albedo, ResourceState::ShaderResource);
				cmd.TransitionTexture(gbuffer.normal, ResourceState::ShaderResource);
				cmd.TransitionTexture(gbuffer.material, ResourceState::ShaderResource);
				cmd.TransitionTexture(gbuffer.emissive, ResourceState::ShaderResource);
				cmd.TransitionTexture(gbuffer.depth, ResourceState::ShaderResource);

				cmd.SetPipelineState(context.pipelines->Get(pipeline));
				cmd.SetPrimitiveTopology(PrimitiveTopology::TriangleList);

				cmd.TransitionTexture(hdr, ResourceState::RenderTarget);
				const DescriptorHandle hdrRTV = hdr->GetRTV();
				cmd.SetRenderTargets(1, &hdrRTV, nullptr);

				BindConstants(cmd, context, 0, constants);

				cmd.SetShaderResources(
					1, { gbuffer.albedo, gbuffer.normal, gbuffer.material, gbuffer.depth, gbuffer.emissive });

				if (!lightInfos.empty())
				{
					const UploadResult lightUpload =
						context.uploadBuffer->AllocAndCopy(lightInfos.data(), lightInfos.size() * sizeof(LightInfo));
					cmd.SetShaderResourceBuffer(2, context.uploadBuffer->GetBackingBuffer(), lightUpload.offset);
				}

				cmd.SetShaderResources(3, { shadowMap });

				cmd.Draw(3);
			});
}
