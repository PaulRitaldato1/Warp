#pragma once

#include <Common/CommonTypes.h>

class Texture;
struct TextureDesc;

class Buffer;
struct BufferDesc;

class SwapChain;
struct SwapChainDesc;

class Shader;
struct ShaderDesc;

class PipelineState;
struct PipelineDesc;

class CommandQueue;

struct DeviceDesc
{
	bool bEnableDebugLayer		   = false;
	bool bEnableGPUValidationLayer = false;
};

struct PhysicalDeviceInfo
{
	bool bSupportsHardwareRT  = false;
	bool bSupportsFP16		  = false;
	bool bSupportsMeshShaders = false;
	u32 SupportedMSSALevel	  = 0;
	u32 MaxTextureWidth;
	u32 MaxTextureHeight;
	u32 MaxComputeWorkGroupSize;
};

class Device
{
public:
	virtual ~Device() = default;

	// Initialize the device
	virtual void Initialize(const DeviceDesc& Desc) = 0;

	// Create a command queue
	virtual URef<CommandQueue> CreateCommandQueue() = 0;

	// Create a swap chain
	virtual URef<SwapChain> CreateSwapChain(const SwapChainDesc& Desc) = 0;

	// Create a pipeline state object
	virtual URef<PipelineState> CreatePipelineState(const PipelineDesc& Desc) = 0;

	// Create a buffer
	virtual URef<Buffer> CreateBuffer(const BufferDesc& Desc) = 0;

	// Create a texture
	virtual URef<Texture> CreateTexture(const TextureDesc& Desc) = 0;

	// Create a shader
	virtual URef<Shader> CreateShader(const ShaderDesc& Desc) = 0;

	// Synchronize the device (wait for all operations to complete)
	virtual void WaitForIdle() = 0;

	virtual const char* GetAPIName() = 0;

private:
	PhysicalDeviceInfo m_DeviceInfo;
};
