#pragma once

#include <Common/CommonTypes.h>
#include <Rendering/Renderer/Pipeline.h>
#include <Rendering/Renderer/Shader.h>

#include <optional>

class Device;

// An index into the library. Stays valid across a reload, unlike the PSO it
// resolves to, so passes keep the handle and look the PSO up when binding.
struct PipelineHandle
{
	static constexpr u32 k_invalid = ~0u;
	u32 index					   = k_invalid;

	bool IsValid() const
	{
		return index != k_invalid;
	}
};

struct ComputePipelineHandle
{
	static constexpr u32 k_invalid = ~0u;
	u32 index					   = k_invalid;

	bool IsValid() const
	{
		return index != k_invalid;
	}
};

struct GraphicsPipelineInfo
{
	String name; // logs, markers, reload errors
	ShaderDesc vertexShader;
	std::optional<ShaderDesc> pixelShader; // the shadow pass has none
	PipelineDesc state;					   // leave the shader pointers null, the library fills them in
};

struct ComputePipelineInfo
{
	String name;
	ShaderDesc computeShader;
	ComputePipelineDesc state; // leave the shader pointer null, the library fills it in
};

// Lighting, tonemap and the post passes: the shared full screen triangle VS, no
// depth, no blending, cull none. Returns the info rather than a handle so the
// pass can still add samplers or swap the VS before calling Create.
GraphicsPipelineInfo MakeFullscreenPipeline(String name, String pixelShaderPath, TextureFormat renderTargetFormat,
											Vector<BindingSlot> bindings);

// Owns every shader and PSO the renderer uses and hands out handles to them.
class PipelineLibrary
{
public:
	explicit PipelineLibrary(Device* device);
	~PipelineLibrary();

	// Called at init. A shader that fails to compile or a PSO the driver rejects
	// is fatal here, so a broken shader stops startup.
	PipelineHandle Create(const GraphicsPipelineInfo& info);
	ComputePipelineHandle Create(const ComputePipelineInfo& info);

	PipelineState* Get(PipelineHandle handle) const;
	ComputePipelineState* Get(ComputePipelineHandle handle) const;

	// Recompiles every shader and rebuilds every PSO. A failed shader keeps the
	// old one, and so does a failed PSO. The GPU must be idle, since the old PSOs
	// are destroyed.
	void ReloadAll();

private:
	// Keyed by type + path + entry point, so passes sharing a shader compile it once.
	// Null if it fails to compile.
	Shader* GetOrCompileShader(const ShaderDesc& desc);

	// Null if a shader or the PSO failed.
	URef<PipelineState> BuildPipeline(const GraphicsPipelineInfo& info);
	URef<ComputePipelineState> BuildPipeline(const ComputePipelineInfo& info);

	struct ShaderEntry
	{
		ShaderDesc desc;
		URef<Shader> shader;
	};

	struct GraphicsEntry
	{
		GraphicsPipelineInfo info;
		URef<PipelineState> pso;
	};

	struct ComputeEntry
	{
		ComputePipelineInfo info;
		URef<ComputePipelineState> pso;
	};

	Device* m_device = nullptr; // non-owning, the renderer owns it

	HashMap<String, ShaderEntry> m_shaders;
	Vector<GraphicsEntry> m_graphics;
	Vector<ComputeEntry> m_compute;
};
