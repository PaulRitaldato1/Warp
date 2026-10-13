#include <Rendering/Passes/CullPass.h>
#include <Rendering/Renderer/RenderGraph.h>

// Matches CullConstants in InstanceCull.hlsl.
struct CullConstants
{
	Vec4 planes[6];
	u32 slotCount;
	u32 requiredFlags;
	u32 padding[2];
};

ComputePipelineHandle CreateCullPassPipeline(PipelineLibrary& library)
{
	ComputePipelineInfo info;
	info.name			= "Instance Cull";
	info.computeShader	= ShaderDesc::FromFile(ShaderType::Compute, "Shaders/InstanceCull.hlsl", "CSMain");
	info.state.bindings = {
		{ BindingType::ConstantBuffer, 0, 1 },	   // rootIndex 0: b0 — frustum, slot count, flags
		{ BindingType::StructuredBuffer, 0, 1 },   // rootIndex 1: t0 — instances
		{ BindingType::StructuredBuffer, 1, 1 },   // rootIndex 2: t1 — region starts
		{ BindingType::RWStructuredBuffer, 0, 1 }, // rootIndex 3: u0 — draw args
		{ BindingType::RWStructuredBuffer, 1, 1 }, // rootIndex 4: u1 — visible indices
	};
	return library.Create(info);
}

void AddCullPass(RenderGraph& graph, const PassContext& context, CullPassInputs inputs)
{
	graph.AddPass("GPU Cull")
		.Execute(
			[context, inputs = std::move(inputs)](CommandList& cmd)
			{
				for (const CullView& view : inputs.views)
				{
					CullConstants constants = {};
					for (u32 plane = 0; plane < 6; ++plane)
					{
						constants.planes[plane] = view.frustum[plane];
					}
					constants.slotCount		= inputs.slotCount;
					constants.requiredFlags = view.requiredFlags;

					cmd.TransitionBuffer(view.drawArgs, ResourceState::UnorderedAccess);
					cmd.TransitionBuffer(view.visibleIndices, ResourceState::UnorderedAccess);

					cmd.SetComputePipelineState(context.pipelines->Get(inputs.pipeline));
					BindConstants(cmd, context, 0, constants);
					cmd.SetShaderResourceBuffer(1, inputs.instances, 0);
					cmd.SetShaderResourceBuffer(2, inputs.regionStarts, 0);
					cmd.SetUnorderedAccessBuffer(3, view.drawArgs, 0);
					cmd.SetUnorderedAccessBuffer(4, view.visibleIndices, 0);
					cmd.Dispatch((inputs.slotCount + 63) / 64, 1, 1);

					// The draw reads the args as indirect arguments, not as a UAV. Missing this
					// transition is a device removal, not a visible bug.
					cmd.TransitionBuffer(view.drawArgs, ResourceState::IndirectArgument);
					cmd.TransitionBuffer(view.visibleIndices, ResourceState::ShaderResource);
				}
			});
}
