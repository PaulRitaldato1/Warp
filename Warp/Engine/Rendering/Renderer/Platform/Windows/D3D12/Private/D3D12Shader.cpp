#ifdef WARP_BUILD_DX12

#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12Shader.h>
#include <Debugging/Assert.h>
#include <Debugging/Logging.h>
#include <Renderer/DxcCommon.h>

#include <cstring>

bool D3D12Shader::Initialize(const ShaderDesc& desc)
{
	DYNAMIC_ASSERT(!desc.entryPoint.empty(), "D3D12Shader::Initialize: entryPoint must not be empty");
	DYNAMIC_ASSERT(!desc.filePath.empty() || !desc.sourceCode.empty(),
				   "D3D12Shader::Initialize: either filePath or sourceCode must be set");

	DxcRef<IDxcUtils> utils;
	DxcRef<IDxcCompiler3> compiler;
	ThrowIfFailed(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils)));
	ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler)));

	DxcRef<IDxcIncludeHandler> includeHandler;
	ThrowIfFailed(utils->CreateDefaultIncludeHandler(&includeHandler));

	// Build the DXC argument list
	std::wstring entryW(desc.entryPoint.begin(), desc.entryPoint.end());
	std::wstring targetW = Warp::Dxc::ToShaderTarget(desc.type);

	Vector<const wchar_t*> args;

	// File name for error message source attribution (optional but helpful)
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

#if defined(WARP_DEBUG)
	args.push_back(L"-Zs"); // embed PDB-style debug info in the DXIL
	args.push_back(L"-Od"); // skip optimisation
#else
	args.push_back(L"-O3");
#endif

	// Source buffer — either loaded from disk or provided inline
	DxcBuffer sourceBuffer = {};
	DxcRef<IDxcBlobEncoding> sourceBlob;

	if (!desc.filePath.empty())
	{
		WString wpath(desc.filePath.begin(), desc.filePath.end());
		if (FAILED(utils->LoadFile(wpath.c_str(), nullptr, &sourceBlob)))
		{
			LOG_ERROR("D3D12Shader: could not load '{}'", desc.filePath);
			return false;
		}
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
	ThrowIfFailed(compiler->Compile(&sourceBuffer, args.data(), static_cast<UINT32>(args.size()), includeHandler.Get(),
									IID_PPV_ARGS(&result)));

	DxcRef<IDxcBlobUtf8> errors;
	result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
	const char* compilerOutput = (errors && errors->GetStringLength() > 0) ? errors->GetStringPointer() : "";

	HRESULT hr;
	result->GetStatus(&hr);

	DxcRef<IDxcBlob> shaderBlob;
	result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);

	if (FAILED(hr) || !shaderBlob || shaderBlob->GetBufferSize() == 0)
	{
		LOG_ERROR("D3D12Shader: {} shader {} in '{}' failed to compile:\n{}", ToString(desc.type), desc.entryPoint,
				  desc.filePath.empty() ? "source string" : desc.filePath, compilerOutput);
		return false;
	}

	// Warnings only, since it compiled.
	if (compilerOutput[0] != '\0')
	{
		LOG_WARNING("D3D12Shader compile output: {}", compilerOutput);
	}

	m_bytecode.resize(shaderBlob->GetBufferSize());
	std::memcpy(m_bytecode.data(), shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize());

	String targetStr(targetW.begin(), targetW.end());
	LOG_DEBUG("D3D12Shader: compiled {} shader {} from '{}' ({}, {} bytes DXIL)", ToString(desc.type),
			  desc.entryPoint, desc.filePath.empty() ? "source string" : desc.filePath, targetStr,
			  m_bytecode.size());
	return true;
}

void D3D12Shader::Cleanup()
{
	m_bytecode.clear();
}

#endif
