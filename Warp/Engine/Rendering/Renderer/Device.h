#pragma once

#include "Renderer/Pipeline.h"
#include <Common/CommonTypes.h>
#include <Rendering/Renderer/CommandQueue.h>

class Texture;
struct TextureDesc;

class Buffer;
struct BufferDesc;

class SwapChain;
struct SwapChainDesc;

class Shader;
struct ShaderDesc;

class PipelineState;
class ComputePipelineState;
struct PipelineDesc;
struct ComputePipelineDesc;

class CommandList;
class UploadBuffer;

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

	// Create a command queue of the given type
	virtual URef<CommandQueue>  CreateCommandQueue(CommandQueueType type) = 0;

	// Create a command list; one allocator slot is allocated per frame-in-flight
	virtual URef<CommandList>   CreateCommandList(CommandQueueType type, u32 framesInFlight) = 0;

	// Create a GPU-visible upload heap backed by a ring buffer
	virtual URef<UploadBuffer>  CreateUploadBuffer(u64 size, u32 framesInFlight) = 0;

	// Create a swap chain
	virtual URef<SwapChain> CreateSwapChain(const SwapChainDesc& Desc) = 0;

	// Create a graphics pipeline state object
	virtual URef<PipelineState> CreatePipelineState(const PipelineDesc& Desc) = 0;

	// Create a compute pipeline state object
	virtual URef<ComputePipelineState> CreateComputePipelineState(const ComputePipelineDesc& Desc) = 0;

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
