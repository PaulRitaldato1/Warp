#pragma once

#include <Common/CommonTypes.h>

enum class ShaderType
{
	Vertex,
	Pixel,
	Compute,
	Geometry,
	Hull,
	Domain,
};

struct ShaderDesc
{
	ShaderType type;
	String     entryPoint = "main";
	String     filePath;       // load from file   (preferred)
	String     sourceCode;     // compile from string (fallback / runtime)
};

class Shader
{
public:
	virtual ~Shader() = default;

	// Compile or load the shader. Logs and asserts on failure.
	virtual void Initialize(const ShaderDesc& desc) = 0;

	// Raw pointer to compiled bytecode (ID3DBlob data, SPIRV words, etc.)
	virtual const void* GetBytecode()     const = 0;
	virtual u64         GetBytecodeSize() const = 0;

	// Generic handle — same as GetBytecode() on most backends.
	virtual void* GetNativeHandle() const = 0;

	virtual void  Cleanup() = 0;
};
