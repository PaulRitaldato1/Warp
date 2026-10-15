#include <Rendering/Passes/GBufferPass.h>
#include <Rendering/Renderer/Device.h>
#include <Rendering/Renderer/FrameBlackboard.h>
#include <Rendering/Renderer/FrameView.h>
#include <Rendering/Renderer/GPUScene.h>
#include <Rendering/Renderer/RenderGraph.h>
#include <Rendering/Renderer/Texture.h>

// Split by update frequency. The per-view half is bound once for the pass; only
// the per-batch half is re-uploaded for each batch.

// Matches PerView in DeferredGeometry.hlsl.
struct PerViewConstants
{
	Mat4 viewProj;
};

// Matches PerBatchConstants in DeferredGeometry.hlsl.
struct PerBatchConstants
{
	Vec3 emissiveFactor;
	uint instanceOffset;
};

static PipelineHandle CreatePipeline(PipelineLibrary& library)
{
	GraphicsPipelineInfo info;
	info.name		  = "GBuffer Geometry";
	info.vertexShader = ShaderDesc::FromFile(ShaderType::Vertex, "Shaders/DeferredGeometry.hlsl", "VSMain");
	info.pixelShader  = ShaderDesc::FromFile(ShaderType::Pixel, "Shaders/DeferredGeometry.hlsl", "PSMain");

	// Slot 0 is the position stream, slot 1 the VertexAttributes struct in Mesh.h.
	// Byte offsets append-align within each slot independently.
	PipelineDesc& meshDesc = info.state;
	meshDesc.inputLayout   = {
		{ "POSITION", 0, TextureFormat::RGB32F, 0, InputElement::AppendAligned },
		{ "NORMAL", 0, TextureFormat::RGB32F, 1, InputElement::AppendAligned },
		{ "TANGENT", 0, TextureFormat::RGBA32F, 1, InputElement::AppendAligned },
		{ "TEXCOORD", 0, TextureFormat::RG32F, 1, InputElement::AppendAligned },
		{ "TEXCOORD", 1, TextureFormat::RG32F, 1, InputElement::AppendAligned },
		{ "COLOR", 0, TextureFormat::RGBA32F, 1, InputElement::AppendAligned },
	};
	// Same order as GBufferOutput.
	meshDesc.renderTargetFormats  = { TextureFormat::RGBA8, TextureFormat::RGBA16F, TextureFormat::RGBA8,
									  TextureFormat::RGBA8 };
	meshDesc.depthFormat		  = TextureFormat::Depth32F;
	meshDesc.topology			  = PrimitiveTopology::TriangleList;
	meshDesc.enableDepthTest	  = true;
	meshDesc.enableDepthWrite	  = true;
	meshDesc.enableStencilTest	  = false;
	meshDesc.enableBlending		  = false;
	meshDesc.rasterState.cullMode = RasterizerState::CullMode::Back;
	meshDesc.rasterState.fillMode = RasterizerState::FillMode::Solid;
	meshDesc.bindings			  = {
		{ BindingType::ConstantBuffer, 0, 1 },								 // rootIndex 0: b0 — per-draw constants
		{ BindingType::TextureTable, 0, TextureSlot::TextureSlotCount },	 // rootIndex 1: t0-t4 — material textures
		{ BindingType::ConstantBuffer, 1, 1 },								 // rootIndex 2: b1 — per-view constants
		{ BindingType::StructuredBuffer, TextureSlot::TextureSlotCount, 1 }, // rootIndex 3: t5 — instance data
		{ BindingType::StructuredBuffer, TextureSlot::TextureSlotCount + 1, 1 }
	}; // rootIndex 4: t6 — slot indices
	meshDesc.samplers = {
		{ 0, SamplerFilter::Linear, SamplerAddressMode::Wrap },
	};

	return library.Create(info);
}

GBufferPass::~GBufferPass() = default;

void GBufferPass::Init(const PassInitContext& context)
{
	m_device   = context.device;
	m_pipeline = CreatePipeline(*context.pipelines);
	CreateTargets(context.width, context.height);
}

void GBufferPass::OnResize(u32 width, u32 height)
{
	CreateTargets(width, height);
}

void GBufferPass::CreateTargets(u32 width, u32 height)
{
	auto createTarget = [&](TextureFormat format, TextureUsage usage)
	{
		TextureDesc desc;
		desc.type	= TextureType::Texture2D;
		desc.width	= width;
		desc.height = height;
		desc.format = format;
		desc.usage	= usage;
		return m_device->CreateTexture(desc);
	};

	m_albedo   = createTarget(TextureFormat::RGBA8, TextureUsage::RenderTarget);
	m_normal   = createTarget(TextureFormat::RGBA16F, TextureUsage::RenderTarget);
	m_material = createTarget(TextureFormat::RGBA8, TextureUsage::RenderTarget);
	m_emissive = createTarget(TextureFormat::RGBA8, TextureUsage::RenderTarget);

	// Sampled by lighting to rebuild world positions.
	m_depth = createTarget(TextureFormat::Depth32F, TextureUsage::DepthStencilSampled);
}

void GBufferPass::AddToGraph(RenderGraph& graph, FrameBlackboard& blackboard, const FrameView& view,
							 const PassContext& context)
{
	const SceneDrawView scene = blackboard.Get<GPUSceneOutput>().camera;

	const GBufferOutput output{ m_albedo.get(), m_normal.get(), m_material.get(), m_emissive.get(), m_depth.get() };
	blackboard.Add(output);

	graph.AddPass("GBuffer").Execute(
		[context, pipeline = m_pipeline, output, scene, viewProj = view.camera.viewProj](CommandList& cmd)
		{
			const Array<DescriptorHandle, 4> rtvs{ output.albedo->GetRTV(), output.normal->GetRTV(),
												   output.material->GetRTV(), output.emissive->GetRTV() };

			cmd.TransitionTexture(output.albedo, ResourceState::RenderTarget);
			cmd.TransitionTexture(output.normal, ResourceState::RenderTarget);
			cmd.TransitionTexture(output.material, ResourceState::RenderTarget);
			cmd.TransitionTexture(output.emissive, ResourceState::RenderTarget);

			// The lighting pass left it as ShaderResource last frame.
			cmd.TransitionTexture(output.depth, ResourceState::DepthWrite);

			cmd.SetRenderTargets(4, rtvs.data(), output.depth);

			for (const DescriptorHandle& rtv : rtvs)
			{
				cmd.ClearRenderTarget(rtv, 0.0f, 0.0f, 0.0f, 0.0f);
			}
			cmd.ClearDepthStencil(output.depth, 1.f, 0);

			const u32 width	 = output.albedo->GetWidth();
			const u32 height = output.albedo->GetHeight();
			cmd.SetViewport(0.f, 0.f, static_cast<f32>(width), static_cast<f32>(height));
			cmd.SetScissorRect(0, 0, width, height);

			cmd.SetPipelineState(context.pipelines->Get(pipeline));
			cmd.SetPrimitiveTopology(PrimitiveTopology::TriangleList);

			BindConstants(cmd, context, 2, PerViewConstants{ viewProj });

			if (!scene.batches.empty())
			{
				cmd.SetShaderResourceBuffer(3, scene.instances, 0);
				cmd.SetShaderResourceBuffer(4, scene.visibleIndices, 0);
			}

			for (const BatchItem& batch : scene.batches)
			{
				PerBatchConstants batchConstants;
				batchConstants.emissiveFactor = batch.emissiveFactor;
				batchConstants.instanceOffset = batch.instanceOffset;
				BindConstants(cmd, context, 0, batchConstants);

				Buffer* streams[] = { batch.positionBuffer, batch.attributeBuffer };
				cmd.SetVertexBuffers(streams, 2);
				cmd.SetIndexBuffer(batch.indexBuffer);
				cmd.SetShaderResources(1, { batch.textures[TextureSlot::BaseColor], batch.textures[TextureSlot::Normal],
											batch.textures[TextureSlot::MetallicRoughness],
											batch.textures[TextureSlot::Occlusion],
											batch.textures[TextureSlot::Emissive] });

				cmd.DrawIndexedIndirect(scene.drawArgs, batch.batchIndex * sizeof(DrawIndexedArgs));
			}
		});
}
