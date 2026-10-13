#include <Rendering/Passes/LightingPass.h>
#include <Rendering/Renderer/RenderGraph.h>
#include <Rendering/Renderer/Texture.h>

PipelineHandle CreateLightingPassPipeline(PipelineLibrary& library)
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

// The scene lights in the GPU layout, then the sky's sun.
static Vector<LightInfo> BuildLightInfos(const LightList& lights, const Vector<LightInfo>& skyLights)
{
	Vector<LightInfo> lightInfos;
	lightInfos.reserve(lights.items.size() + skyLights.size());

	for (const LightItem& light : lights.items)
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

	lightInfos.insert(lightInfos.end(), skyLights.begin(), skyLights.end());
	return lightInfos;
}

void AddLightingPass(RenderGraph& graph, const PassContext& context, const LightingPassInputs& inputs)
{
	// Built while the pass is added, so the lambda only records.
	Vector<LightInfo> lightInfos = BuildLightInfos(inputs.lights, inputs.skyLights);

	LightPassConstants constants = {};
	{
		using namespace DirectX;
		SimdMat simdViewProj	= XMLoadFloat4x4(&inputs.viewProj);
		SimdMat simdInvViewProj = XMMatrixInverse(nullptr, simdViewProj);
		XMStoreFloat4x4(&constants.invViewProj, simdInvViewProj);
	}
	constants.cameraPosition = inputs.cameraPosition;
	constants.lightCount	 = static_cast<int32>(lightInfos.size());
	constants.sky			 = inputs.sky;
	constants.lightViewProj	 = inputs.lightViewProj;
	constants.shadowBias	 = 0.005f;

	graph.AddPass("Lighting")
		.Execute(
			[context, pipeline = inputs.pipeline, gbuffer = inputs.gbuffer, depth = inputs.depth,
			 shadowMap = inputs.shadowMap, hdrTarget = inputs.hdrTarget, constants,
			 lightInfos = std::move(lightInfos)](CommandList& cmd)
			{
				cmd.TransitionTexture(gbuffer.albedo, ResourceState::ShaderResource);
				cmd.TransitionTexture(gbuffer.normal, ResourceState::ShaderResource);
				cmd.TransitionTexture(gbuffer.material, ResourceState::ShaderResource);
				cmd.TransitionTexture(gbuffer.emissive, ResourceState::ShaderResource);
				cmd.TransitionTexture(depth, ResourceState::ShaderResource);

				cmd.SetPipelineState(context.pipelines->Get(pipeline));
				cmd.SetPrimitiveTopology(PrimitiveTopology::TriangleList);

				cmd.TransitionTexture(hdrTarget, ResourceState::RenderTarget);
				const DescriptorHandle hdrRTV = hdrTarget->GetRTV();
				cmd.SetRenderTargets(1, &hdrRTV, nullptr);

				BindConstants(cmd, context, 0, constants);

				cmd.SetShaderResources(1,
									   { gbuffer.albedo, gbuffer.normal, gbuffer.material, depth, gbuffer.emissive });

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
