#ifdef WARP_BUILD_VK

#include <Rendering/Renderer/Platform/Vulkan/VKShader.h>
#include <Debugging/Assert.h>
#include <Debugging/Logging.h>
#include <Renderer/DxcCommon.h>
#include <cstring>

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
	DYNAMIC_ASSERT(!desc.entryPoint.empty(), "VKShader::Initialize: entryPoint must not be empty");
	DYNAMIC_ASSERT(!desc.filePath.empty() || !desc.sourceCode.empty(),
				   "VKShader::Initialize: either filePath or sourceCode must be set");

	using namespace Warp::Dxc;

	DxcCreateInstanceProc createInstance = GetDxcCreateInstance();
	FATAL_ASSERT(createInstance, "VKShader::Initialize: Failed the GetDxcCreateInstance");

	DxcRef<IDxcUtils> utils;
	DxcRef<IDxcCompiler3> compiler;
	FATAL_ASSERT(SUCCEEDED(createInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils))),
				 "VKShader::Initialize: Failed to Create DXC Utils");

	FATAL_ASSERT(SUCCEEDED(createInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler))),
				 "VKShader::Initialize: Failed to create DXC compiler");

	DxcRef<IDxcIncludeHandler> includeHandler;
	FATAL_ASSERT(SUCCEEDED(utils->CreateDefaultIncludeHandler(&includeHandler)),
				 "VKShader::Initialize: Failed to create include handler");

	std::wstring entryW(desc.entryPoint.begin(), desc.entryPoint.end());
	std::wstring targetW = ToShaderTarget(desc.type);

	Vector<const wchar_t*> args;

	std::wstring nameW;

	if (!desc.filePath.empty())
	{
		nameW = std::wstring(desc.filePath.begin(), desc.filePath.end());
		args.push_back(nameW.c_str());
	}

	args.push_back(L"-E");
	args.push_back(entryW.c_str());
	args.push_back(L"-T");
	args.push_back(targetW.c_str());

	args.push_back(L"-spirv");

	args.push_back(L"-fvk-b-shift");
	args.push_back(toWString(VkBindingShift::B));
	args.push_back(L"all");

	args.push_back(L"-fvk-t-shift");
	args.push_back(toWString(VkBindingShift::T));
	args.push_back(L"all");

	args.push_back(L"-fvk-u-shift");
	args.push_back(toWString(VkBindingShift::U));
	args.push_back(L"all");

	args.push_back(L"-fvk-s-shift");
	args.push_back(toWString(VkBindingShift::S));
	args.push_back(L"all");

	args.push_back(L"-fspv-target-env=vulkan1.3");

#if defined(WARP_DEBUG)
	args.push_back(L"-Zi");
	args.push_back(L"-Od");
#else
	args.push_back(L"-O3");
#endif

	DxcBuffer sourceBuffer = {};
	DxcRef<IDxcBlobEncoding> sourceBlob;

	if (!desc.filePath.empty())
	{
		WString wpath(desc.filePath.begin(), desc.filePath.end());
		FATAL_ASSERT(SUCCEEDED(utils->LoadFile(wpath.c_str(), nullptr, &sourceBlob)),
					 "VKShader::Initialize: Failed to load file");

		sourceBuffer.Ptr	  = sourceBlob->GetBufferPointer();
		sourceBuffer.Size	  = sourceBlob->GetBufferSize();
		sourceBuffer.Encoding = DXC_CP_ACP;
	}
	else
	{
		sourceBuffer.Ptr	  = desc.sourceCode.c_str();
		sourceBuffer.Size	  = desc.sourceCode.size();
		sourceBuffer.Encoding = DXC_CP_UTF8;
	}

	DxcRef<IDxcResult> result;
	FATAL_ASSERT(SUCCEEDED(compiler->Compile(&sourceBuffer, args.data(), static_cast<UINT32>(args.size()),
											 includeHandler.Get(), IID_PPV_ARGS(&result))),
				 "VKShader::Initalize: Failed to Compile");

	DxcRef<IDxcBlobUtf8> errors;
	result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
	if (errors && errors->GetStringLength() > 0)
	{
		LOG_WARNING("VKShader compile output: {}", errors->GetStringPointer());
	}

	HRESULT hr;
	result->GetStatus(&hr);
	DYNAMIC_ASSERT(SUCCEEDED(hr), "VKShader::Initialize: shader compilation failed");

	DxcRef<IDxcBlob> shaderBlob;
	result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	DYNAMIC_ASSERT(shaderBlob && shaderBlob->GetBufferSize() > 0, "VKShader::Initialize: No spirv output produced");

	m_spirv.resize(shaderBlob->GetBufferSize() / sizeof(u32));
	std::memcpy(m_spirv.data(), shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize());

	VkShaderModuleCreateInfo moduleInfo = {};
	moduleInfo.sType					= VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	moduleInfo.codeSize					= shaderBlob->GetBufferSize();
	moduleInfo.pCode					= m_spirv.data();

	VK_CHECK(vkCreateShaderModule(m_device, &moduleInfo, nullptr, &m_module), "VKShader: vkCreateShaderModule failed");

	LOG_DEBUG("VKShader: compiled '{}' ({} bytes SPIR-V)", desc.filePath, m_spirv.size() * sizeof(u32));
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
