#ifdef WARP_BUILD_VK

#include <Rendering/Renderer/Platform/Vulkan/VKShader.h>
#include <Debugging/Assert.h>
#include <Debugging/Logging.h>
#include <shaderc/shaderc.hpp>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static shaderc_shader_kind ToShadercKind(ShaderType type)
{
	switch (type)
	{
		case ShaderType::Vertex:   return shaderc_vertex_shader;
		case ShaderType::Pixel:    return shaderc_fragment_shader;
		case ShaderType::Compute:  return shaderc_compute_shader;
		case ShaderType::Geometry: return shaderc_geometry_shader;
		case ShaderType::Hull:     return shaderc_tess_control_shader;
		case ShaderType::Domain:   return shaderc_tess_evaluation_shader;
		default:                   return shaderc_vertex_shader;
	}
}

// ---------------------------------------------------------------------------
// VKShader
// ---------------------------------------------------------------------------

VKShader::~VKShader()
{
	Cleanup();
}

void VKShader::InitializeWithDevice(VkDevice device)
{
	DYNAMIC_ASSERT(device, "VKShader: device is null");
	m_device = device;
}

void VKShader::Initialize(const ShaderDesc& desc)
{
	// Select source — prefer embedded source string, fall back to file path.
	String source;
	String sourceName = "shader";

	if (!desc.sourceCode.empty())
	{
		source     = desc.sourceCode;
		sourceName = "embedded";
	}
	else if (!desc.filePath.empty())
	{
		sourceName = desc.filePath;
		FILE* f = fopen(desc.filePath.c_str(), "rb");
		DYNAMIC_ASSERT(f, ("VKShader: could not open file: " + desc.filePath).c_str());
		fseek(f, 0, SEEK_END);
		long len = ftell(f);
		fseek(f, 0, SEEK_SET);
		source.resize(static_cast<size_t>(len));
		fread(source.data(), 1, static_cast<size_t>(len), f);
		fclose(f);
	}
	else
	{
		FATAL_ASSERT(false, "VKShader: no source code or file path provided");
	}

	shaderc::Compiler       compiler;
	shaderc::CompileOptions options;

	options.SetSourceLanguage(shaderc_source_language_hlsl);
	options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
	options.SetTargetSpirv(shaderc_spirv_version_1_6);
	// Entry point is passed as the final argument to CompileGlslToSpv below.

#if defined(WARP_DEBUG)
	options.SetGenerateDebugInfo();
	options.SetOptimizationLevel(shaderc_optimization_level_zero);
#else
	options.SetOptimizationLevel(shaderc_optimization_level_performance);
#endif

	shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(
		source.c_str(), source.size(),
		ToShadercKind(desc.type),
		sourceName.c_str(),
		desc.entryPoint.c_str(),
		options);

	if (result.GetCompilationStatus() != shaderc_compilation_status_success)
	{
		LOG_ERROR("VKShader: HLSL compilation failed: " + String(result.GetErrorMessage()));
		FATAL_ASSERT(false, "VKShader: shader compilation failed");
		return;
	}

	if (result.GetNumWarnings() > 0)
	{
		LOG_WARNING("VKShader: " + String(result.GetErrorMessage()));
	}

	m_spirv.assign(result.cbegin(), result.cend());

	VkShaderModuleCreateInfo moduleInfo = {};
	moduleInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	moduleInfo.codeSize = m_spirv.size() * sizeof(u32);
	moduleInfo.pCode    = m_spirv.data();

	VK_CHECK(vkCreateShaderModule(m_device, &moduleInfo, nullptr, &m_module),
	         "VKShader: vkCreateShaderModule failed");

	LOG_DEBUG("VKShader: compiled '" + sourceName + "' (" +
	          std::to_string(m_spirv.size() * 4) + " bytes SPIR-V)");
}

void VKShader::Cleanup()
{
	if (m_module != VK_NULL_HANDLE)
	{
		vkDestroyShaderModule(m_device, m_module, nullptr);
		m_module = VK_NULL_HANDLE;
	}
	m_spirv.clear();
}

#endif // WARP_BUILD_VK
