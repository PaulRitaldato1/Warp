#pragma once

#ifdef WARP_BUILD_VK

#include <Rendering/Renderer/Shader.h>
#include <Rendering/Renderer/Platform/Vulkan/VKCommon.h>

// ---------------------------------------------------------------------------
// VKShader — compiles HLSL source (or loads pre-compiled SPIR-V) using
// shaderc on Linux; the resulting SPIR-V is wrapped in a VkShaderModule.
// ---------------------------------------------------------------------------

class VKShader : public Shader
{
public:
	~VKShader() override;

	void InitializeWithDevice(VkDevice device);

	void        Initialize(const ShaderDesc& desc) override;
	const void* GetBytecode()                const override { return m_spirv.data(); }
	u64         GetBytecodeSize()            const override { return m_spirv.size() * sizeof(u32); }
	void*       GetNativeHandle()            const override { return (void*)m_module; }
	void        Cleanup()                          override;

	VkShaderModule GetModule() const { return m_module; }

private:
	VkDevice       m_device = VK_NULL_HANDLE;
	VkShaderModule m_module = VK_NULL_HANDLE;
	Vector<u32>    m_spirv;
};

#endif // WARP_BUILD_VK
