#include <Debugging/Assert.h>
#include <Debugging/Logging.h>
#include <Rendering/Renderer/Device.h>
#include <Rendering/Renderer/PipelineLibrary.h>

#include <format>

static String MakeShaderKey(const ShaderDesc& desc)
{
	return std::format("{}|{}|{}", ToString(desc.type), desc.filePath, desc.entryPoint);
}

GraphicsPipelineInfo MakeFullscreenPipeline(String name, String pixelShaderPath, TextureFormat renderTargetFormat,
											Vector<BindingSlot> bindings)
{
	GraphicsPipelineInfo info;
	info.name		  = std::move(name);
	info.vertexShader = ShaderDesc::FromFile(ShaderType::Vertex, "Shaders/FullscreenTriangle.hlsl", "VSMain");
	info.pixelShader  = ShaderDesc::FromFile(ShaderType::Pixel, std::move(pixelShaderPath), "PSMain");

	// No input layout: the VS builds the triangle from SV_VertexID.
	info.state.renderTargetFormats	= { renderTargetFormat };
	info.state.depthFormat			= TextureFormat::Unknown;
	info.state.enableDepthTest		= false;
	info.state.enableDepthWrite		= false;
	info.state.rasterState.cullMode = RasterizerState::CullMode::None;
	info.state.bindings				= std::move(bindings);
	return info;
}

PipelineLibrary::PipelineLibrary(Device* device)
	: m_device(device)
{
	DYNAMIC_ASSERT(m_device, "PipelineLibrary: device is null");
}

PipelineLibrary::~PipelineLibrary() = default;

PipelineHandle PipelineLibrary::Create(const GraphicsPipelineInfo& info)
{
	DYNAMIC_ASSERT(!info.state.vertexShader && !info.state.pixelShader,
				   "PipelineLibrary: set shaders through the info's ShaderDescs, not the PipelineDesc");

	URef<PipelineState> pso = BuildPipeline(info);
	if (!pso)
	{
		LOG_ERROR("PipelineLibrary: failed to create {}", info.name);
	}
	FATAL_ASSERT(pso, "PipelineLibrary: a pipeline failed to create at startup, see the log above");

	PipelineHandle handle;
	handle.index = static_cast<u32>(m_graphics.size());
	m_graphics.push_back({ info, std::move(pso) });

	LOG_DEBUG("PipelineLibrary: {} ready", info.name);
	return handle;
}

ComputePipelineHandle PipelineLibrary::Create(const ComputePipelineInfo& info)
{
	DYNAMIC_ASSERT(!info.state.computeShader,
				   "PipelineLibrary: set the shader through the info's ShaderDesc, not the ComputePipelineDesc");

	URef<ComputePipelineState> pso = BuildPipeline(info);
	if (!pso)
	{
		LOG_ERROR("PipelineLibrary: failed to create {}", info.name);
	}
	FATAL_ASSERT(pso, "PipelineLibrary: a pipeline failed to create at startup, see the log above");

	ComputePipelineHandle handle;
	handle.index = static_cast<u32>(m_compute.size());
	m_compute.push_back({ info, std::move(pso) });

	LOG_DEBUG("PipelineLibrary: {} ready", info.name);
	return handle;
}

PipelineState* PipelineLibrary::Get(PipelineHandle handle) const
{
	DYNAMIC_ASSERT(handle.index < m_graphics.size(), "PipelineLibrary::Get: invalid graphics pipeline handle");
	return m_graphics[handle.index].pso.get();
}

ComputePipelineState* PipelineLibrary::Get(ComputePipelineHandle handle) const
{
	DYNAMIC_ASSERT(handle.index < m_compute.size(), "PipelineLibrary::Get: invalid compute pipeline handle");
	return m_compute[handle.index].pso.get();
}

void PipelineLibrary::ReloadAll()
{
	u32 failedShaders = 0;
	for (auto& [key, entry] : m_shaders)
	{
		// The backend logs the compiler error, and the old shader stays.
		URef<Shader> shader = m_device->CreateShader(entry.desc);
		if (!shader)
		{
			++failedShaders;
			continue;
		}
		entry.shader = std::move(shader);
	}

	// Every PSO is rebuilt, not just the ones whose shaders changed. There are few
	// enough that tracking which changed is not worth it.
	u32 failedPipelines = 0;
	for (GraphicsEntry& entry : m_graphics)
	{
		URef<PipelineState> pso = BuildPipeline(entry.info);
		if (!pso)
		{
			LOG_ERROR("PipelineLibrary: {} failed to rebuild, keeping the old PSO", entry.info.name);
			++failedPipelines;
			continue;
		}
		entry.pso = std::move(pso);
	}

	for (ComputeEntry& entry : m_compute)
	{
		URef<ComputePipelineState> pso = BuildPipeline(entry.info);
		if (!pso)
		{
			LOG_ERROR("PipelineLibrary: {} failed to rebuild, keeping the old PSO", entry.info.name);
			++failedPipelines;
			continue;
		}
		entry.pso = std::move(pso);
	}

	if (failedShaders == 0 && failedPipelines == 0)
	{
		LOG_INFO("PipelineLibrary: reloaded {} shaders and {} pipelines", m_shaders.size(),
				 m_graphics.size() + m_compute.size());
	}
	else
	{
		LOG_WARNING("PipelineLibrary: reload kept the old version of {} shaders and {} pipelines, see the errors above",
					failedShaders, failedPipelines);
	}
}

Shader* PipelineLibrary::GetOrCompileShader(const ShaderDesc& desc)
{
	DYNAMIC_ASSERT(!desc.filePath.empty(), "PipelineLibrary: shaders are reloaded from disk, so they need a filePath");

	const String key = MakeShaderKey(desc);
	auto it			 = m_shaders.find(key);
	if (it != m_shaders.end())
	{
		return it->second.shader.get();
	}

	URef<Shader> shader = m_device->CreateShader(desc);
	if (!shader)
	{
		return nullptr;
	}

	Shader* result = shader.get();
	m_shaders.emplace(key, ShaderEntry{ desc, std::move(shader) });
	return result;
}

URef<PipelineState> PipelineLibrary::BuildPipeline(const GraphicsPipelineInfo& info)
{
	PipelineDesc desc = info.state;
	desc.vertexShader = GetOrCompileShader(info.vertexShader);
	desc.pixelShader  = info.pixelShader ? GetOrCompileShader(*info.pixelShader) : nullptr;

	if (!desc.vertexShader || (info.pixelShader && !desc.pixelShader))
	{
		return nullptr;
	}

	return m_device->CreatePipelineState(desc);
}

URef<ComputePipelineState> PipelineLibrary::BuildPipeline(const ComputePipelineInfo& info)
{
	ComputePipelineDesc desc = info.state;
	desc.computeShader		 = GetOrCompileShader(info.computeShader);

	if (!desc.computeShader)
	{
		return nullptr;
	}

	return m_device->CreateComputePipelineState(desc);
}
