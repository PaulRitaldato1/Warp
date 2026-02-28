#pragma once

#include <Rendering/Renderer/Shader.h>

#ifdef WARP_WINDOWS

class D3D12Shader : public Shader
{
public:
	// Shader interface
	void Initialize(const ShaderDesc& desc) override;
	void Cleanup() override;

	const void* GetBytecode()     const override { return m_bytecode.data(); }
	u64         GetBytecodeSize() const override { return static_cast<u64>(m_bytecode.size()); }
	void*       GetNativeHandle() const override { return const_cast<void*>(static_cast<const void*>(m_bytecode.data())); }

private:
	Vector<u8> m_bytecode;
};

#endif
