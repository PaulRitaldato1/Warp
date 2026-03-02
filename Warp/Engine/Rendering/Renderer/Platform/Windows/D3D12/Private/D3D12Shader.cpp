#ifdef WARP_BUILD_DX12

#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12Shader.h>
#include <Debugging/Assert.h>
#include <Debugging/Logging.h>

#include <dxcapi.h>
#include <cstring>

// Maps ShaderType to a DXC target profile string (shader model 6_0 baseline).
static std::wstring ToShaderTarget(ShaderType type)
{
	switch (type)
	{
		case ShaderType::Vertex:   return L"vs_6_0";
		case ShaderType::Pixel:    return L"ps_6_0";
		case ShaderType::Compute:  return L"cs_6_0";
		case ShaderType::Geometry: return L"gs_6_0";
		case ShaderType::Hull:     return L"hs_6_0";
		case ShaderType::Domain:   return L"ds_6_0";
		default:
			DYNAMIC_ASSERT(false, "D3D12Shader: unknown ShaderType");
			return L"vs_6_0";
	}
}

void D3D12Shader::Initialize(const ShaderDesc& desc)
{
	DYNAMIC_ASSERT(!desc.entryPoint.empty(),
				   "D3D12Shader::Initialize: entryPoint must not be empty");
	DYNAMIC_ASSERT(!desc.filePath.empty() || !desc.sourceCode.empty(),
				   "D3D12Shader::Initialize: either filePath or sourceCode must be set");

	ComRef<IDxcUtils>    utils;
	ComRef<IDxcCompiler3> compiler;
	ThrowIfFailed(DxcCreateInstance(CLSID_DxcUtils,    IID_PPV_ARGS(&utils)));
	ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler)));

	ComRef<IDxcIncludeHandler> includeHandler;
	ThrowIfFailed(utils->CreateDefaultIncludeHandler(&includeHandler));

	// Build the DXC argument list
	std::wstring entryW(desc.entryPoint.begin(), desc.entryPoint.end());
	std::wstring targetW = ToShaderTarget(desc.type);

	Vector<const wchar_t*> args;

	// File name for error message source attribution (optional but helpful)
	std::wstring nameW;
	if (!desc.filePath.empty())
	{
		nameW = std::wstring(desc.filePath.begin(), desc.filePath.end());
		args.push_back(nameW.c_str());
	}

	args.push_back(L"-E"); args.push_back(entryW.c_str());
	args.push_back(L"-T"); args.push_back(targetW.c_str());

#if defined(WARP_DEBUG)
	args.push_back(L"-Zs"); // embed PDB-style debug info in the DXIL
	args.push_back(L"-Od"); // skip optimisation
#else
	args.push_back(L"-O3");
#endif

	// Source buffer — either loaded from disk or provided inline
	DxcBuffer           sourceBuffer = {};
	ComRef<IDxcBlobEncoding> sourceBlob;

	if (!desc.filePath.empty())
	{
		WString wpath(desc.filePath.begin(), desc.filePath.end());
		ThrowIfFailed(utils->LoadFile(wpath.c_str(), nullptr, &sourceBlob));
		sourceBuffer.Ptr      = sourceBlob->GetBufferPointer();
		sourceBuffer.Size     = sourceBlob->GetBufferSize();
		sourceBuffer.Encoding = DXC_CP_ACP;
	}
	else
	{
		sourceBuffer.Ptr      = desc.sourceCode.c_str();
		sourceBuffer.Size     = desc.sourceCode.size();
		sourceBuffer.Encoding = DXC_CP_UTF8;
	}

	ComRef<IDxcResult> result;
	ThrowIfFailed(compiler->Compile(
		&sourceBuffer,
		args.data(),
		static_cast<UINT32>(args.size()),
		includeHandler.Get(),
		IID_PPV_ARGS(&result)));

	// Log any error/warning text from the compiler
	ComRef<IDxcBlobUtf8> errors;
	result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
	if (errors && errors->GetStringLength() > 0)
	{
		LOG_WARNING("D3D12Shader compile output: {}", errors->GetStringPointer());
	}

	HRESULT hr;
	result->GetStatus(&hr);
	DYNAMIC_ASSERT(SUCCEEDED(hr), "D3D12Shader::Initialize: shader compilation failed");

	// Extract the DXIL bytecode blob
	ComRef<IDxcBlob> shaderBlob;
	result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	DYNAMIC_ASSERT(shaderBlob && shaderBlob->GetBufferSize() > 0,
				   "D3D12Shader::Initialize: no DXIL output produced");

	m_bytecode.resize(shaderBlob->GetBufferSize());
	std::memcpy(m_bytecode.data(), shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize());

	String targetStr(targetW.begin(), targetW.end());
	LOG_DEBUG("D3D12Shader compiled: {} ({}, {} bytes)", desc.entryPoint, targetStr, m_bytecode.size());
}

void D3D12Shader::Cleanup()
{
	m_bytecode.clear();
}

#endif
