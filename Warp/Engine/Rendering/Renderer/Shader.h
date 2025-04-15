#pragma once

#include <Common/CommonTypes.h>

enum class ShaderType
{
	Vertex,
	Pixel,
	Compute,
	Geometry,
	Hull,
	Domain
};

struct ShaderDesc
{
	ShaderType Type;
	String SourceCode;
	String EntryPoint;
	String FilePath;
};

class Shader
{
public:
	// Initialize the shader
	virtual void Initialize(const ShaderDesc& Desc) = 0;

	// Get native shader handle (platform-specific)
	virtual void* GetNativeHandle() const = 0;

	// Bind the shader to the pipeline
	virtual void Bind() const = 0;

	// Cleanup resources
	virtual void Cleanup() = 0;

	virtual ~Shader() = default;

protected:
	virtual String ShaderTypeToTarget(ShaderType Type) = 0;

private:
};