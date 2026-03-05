#pragma once

#include <Rendering/Renderer/Device.h>
#include <Rendering/Renderer/CommandList.h>
#include <Rendering/Renderer/UploadBuffer.h>

#ifdef WARP_WINDOWS

#include <d3d12.h>
#include <dxgi1_4.h>

class D3D12Device : public Device
{
public:
	const char* GetAPIName() override
	{
		return "D3D12";
	}

	void Initialize(const DeviceDesc& desc) override;

	URef<CommandQueue> CreateCommandQueue(CommandQueueType type) override;
	URef<CommandList> CreateCommandList(CommandQueueType type, u32 framesInFlight) override;
	URef<UploadBuffer> CreateUploadBuffer(u64 size, u32 framesInFlight) override;
	URef<SwapChain> CreateSwapChain(const SwapChainDesc& desc) override;
	URef<PipelineState> CreatePipelineState(const PipelineDesc& desc) override;
	URef<ComputePipelineState> CreateComputePipelineState(const ComputePipelineDesc& desc) override;
	URef<Buffer> CreateBuffer(const BufferDesc& desc) override;
	URef<Texture> CreateTexture(const TextureDesc& desc) override;
	URef<Shader> CreateShader(const ShaderDesc& desc) override;

	void WaitForIdle() override;

	ID3D12Device2* GetNativeDevice() const
	{
		return m_device.Get();
	}
	IDXGIFactory4* GetNativeFactory() const
	{
		return m_factory.Get();
	}

private:
	ComRef<ID3D12Device2> m_device;
	ComRef<IDXGIFactory4> m_factory;
	ComRef<IDXGIAdapter1> m_adapter;
	ComRef<ID3D12Debug> m_debugController;
	ComRef<ID3D12CommandQueue> m_graphicsQueue;
};

#endif
