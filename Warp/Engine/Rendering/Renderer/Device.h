#pragma once

#include <Common/CommonTypes.h>

class ITexture;
struct TextureDesc;

class IBuffer;
struct BufferDesc;

class ISwapChain;
struct SwapChainDesc;

class IShader;
struct ShaderDesc;

class IPipelineState;
struct PipelineDesc;

class ICommandQueue;

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

class IDevice
{
public:
	virtual ~IDevice() = default;

	virtual void Initialize(const DeviceDesc& Desc) = 0;

	// Initialize the device
	virtual void Initialize(const DeviceDesc& Desc) = 0;

	// Create a command queue
	virtual URef<ICommandQueue> CreateCommandQueue() = 0;

	// Create a swap chain
	virtual URef<ISwapChain> CreateSwapChain(const SwapChainDesc& Desc) = 0;

	// Create a pipeline state object
	virtual URef<IPipelineState> CreatePipelineState(const PipelineDesc& Desc) = 0;

	// Create a buffer
	virtual URef<IBuffer> CreateBuffer(const BufferDesc& Desc) = 0;

	// Create a texture
	virtual URef<ITexture> CreateTexture(const TextureDesc& Desc) = 0;

	// Create a shader
	virtual URef<IShader> CreateShader(const ShaderDesc& Desc) = 0;

	// Synchronize the device (wait for all operations to complete)
	virtual void WaitForIdle() = 0;

	virtual const char* GetAPIName() = 0;

private:
	PhysicalDeviceInfo m_DeviceInfo;
};
