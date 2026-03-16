#include "Common/CommonTypes.h"
#include "Renderer/CommandQueue.h"

#ifdef WARP_BUILD_DX12

#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12Device.h>
#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12CommandQueue.h>
#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12CommandList.h>
#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12UploadBuffer.h>
#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12SwapChain.h>
#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12Fence.h>
#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12Buffer.h>
#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12Texture.h>
#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12Shader.h>
#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12Pipeline.h>
#include <Debugging/Assert.h>
#include <Debugging/Logging.h>

#include <dxgi1_6.h>

void D3D12Device::Initialize(const DeviceDesc& desc)
{
	UINT dxgiFactoryFlags = 0;

	if (desc.bEnableDebugLayer)
	{
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&m_debugController))))
		{
			m_debugController->EnableDebugLayer();
			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
			LOG_DEBUG("D3D12 debug layer enabled");
		}
		else
		{
			LOG_WARNING("D3D12GetDebugInterface failed — debug layer not available");
		}
	}

	ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_factory)));

	// Pick the first hardware adapter
	ComRef<IDXGIAdapter1> adapter;
	for (UINT i = 0; m_factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		DXGI_ADAPTER_DESC1 adapterDesc;
		adapter->GetDesc1(&adapterDesc);

		// Skip software adapters
		if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			continue;
		}

		// Check D3D12 support without creating the device
		if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
		{
			m_adapter = adapter;
			break;
		}
	}

	DYNAMIC_ASSERT(m_adapter, "D3D12Device::Initialize: no suitable hardware adapter found");

	ThrowIfFailed(D3D12CreateDevice(m_adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));

	{
		DXGI_ADAPTER_DESC1 adapterDesc;
		m_adapter->GetDesc1(&adapterDesc);
		WString wname(adapterDesc.Description);
		String name(wname.begin(), wname.end());
		LOG_DEBUG("D3D12Device initialized on: {}", name);
	}

	m_srvHeap.Initialize(m_device.Get(), desc.srvHeapCapacity);
}

static D3D12_COMMAND_LIST_TYPE ToD3D12QueueType(CommandQueueType type)
{
	switch (type)
	{
		case CommandQueueType::Graphics:
			return D3D12_COMMAND_LIST_TYPE_DIRECT;
		case CommandQueueType::Compute:
			return D3D12_COMMAND_LIST_TYPE_COMPUTE;
		case CommandQueueType::Copy:
			return D3D12_COMMAND_LIST_TYPE_COPY;
	}
	return D3D12_COMMAND_LIST_TYPE_DIRECT;
}

URef<CommandQueue> D3D12Device::CreateCommandQueue(CommandQueueType type)
{
	DYNAMIC_ASSERT(m_device, "D3D12Device::CreateCommandQueue: device not initialized");

	if (type == CommandQueueType::Graphics && m_graphicsQueue != nullptr)
	{
		LOG_WARNING(
			"D3D12Device::CreateCommandQueue: graphics queue already exists, cannot create another one. You must "
			"recreate the swapchain with this new graphics queue");
	}

	URef<D3D12CommandQueue> queue = std::make_unique<D3D12CommandQueue>();
	queue->InitializeWithDevice(m_device.Get(), ToD3D12QueueType(type));

	// We need to know the graphics queue for swapchain creation
	if (type == CommandQueueType::Graphics)
	{
		m_graphicsQueue = queue->GetNative();
	}

	return queue;
}

URef<CommandList> D3D12Device::CreateCommandList(CommandQueueType type, u32 framesInFlight)
{
	DYNAMIC_ASSERT(m_device, "D3D12Device::CreateCommandList: device not initialized");

	URef<D3D12CommandList> list = std::make_unique<D3D12CommandList>();
	list->InitializeWithDevice(m_device.Get(), framesInFlight, ToD3D12QueueType(type));

	if (type == CommandQueueType::Graphics)
	{
		list->SetDevice(m_device.Get());
		list->SetSRVHeap(&m_srvHeap);
	}

	return list;
}

URef<UploadBuffer> D3D12Device::CreateUploadBuffer(u64 size, u32 framesInFlight)
{
	DYNAMIC_ASSERT(m_device, "D3D12Device::CreateUploadBuffer: device not initialized");

	URef<D3D12UploadBuffer> buffer = std::make_unique<D3D12UploadBuffer>();
	buffer->InitializeWithDevice(m_device.Get(), size, framesInFlight);
	return buffer;
}

URef<SwapChain> D3D12Device::CreateSwapChain(const SwapChainDesc& desc)
{
	DYNAMIC_ASSERT(m_device, "D3D12Device::CreateSwapChain: device not initialized");
	DYNAMIC_ASSERT(m_factory, "D3D12Device::CreateSwapChain: factory not initialized");
	DYNAMIC_ASSERT(m_graphicsQueue, "D3D12Device::CreateSwapChain: graphics queue not initialized, D3D12 requires a "
									"graphics queue for swapchain creation");

	ID3D12CommandQueue* queue	   = static_cast<ID3D12CommandQueue*>(m_graphicsQueue.Get());
	URef<D3D12SwapChain> swapChain = std::make_unique<D3D12SwapChain>();
	swapChain->InitializeWithFactory(m_device.Get(), m_factory.Get(), queue, desc);
	return swapChain;
}

URef<PipelineState> D3D12Device::CreatePipelineState(const PipelineDesc& desc)
{
	DYNAMIC_ASSERT(m_device, "D3D12Device::CreatePipelineState: device not initialized");
	URef<D3D12Pipeline> pipeline = std::make_unique<D3D12Pipeline>();
	pipeline->InitializeWithDevice(m_device.Get(), desc);
	return pipeline;
}

URef<ComputePipelineState> D3D12Device::CreateComputePipelineState(const ComputePipelineDesc& desc)
{
	DYNAMIC_ASSERT(m_device, "D3D12Device::CreateComputePipelineState: device not initialized");
	URef<D3D12ComputePipeline> pipeline = std::make_unique<D3D12ComputePipeline>();
	pipeline->InitializeWithDevice(m_device.Get(), desc);
	return pipeline;
}

URef<Buffer> D3D12Device::CreateBuffer(const BufferDesc& desc)
{
	DYNAMIC_ASSERT(m_device, "D3D12Device::CreateBuffer: device not initialized");
	URef<D3D12Buffer> buffer = std::make_unique<D3D12Buffer>();
	buffer->InitializeWithDevice(m_device.Get(), desc);
	return buffer;
}

URef<Texture> D3D12Device::CreateTexture(const TextureDesc& desc)
{
	DYNAMIC_ASSERT(m_device, "D3D12Device::CreateTexture: device not initialized");
	URef<D3D12Texture> texture = std::make_unique<D3D12Texture>();
	texture->InitializeWithDevice(m_device.Get(), desc);
	return texture;
}

URef<Shader> D3D12Device::CreateShader(const ShaderDesc& desc)
{
	DYNAMIC_ASSERT(m_device, "D3D12Device::CreateShader: device not initialized");
	URef<D3D12Shader> shader = std::make_unique<D3D12Shader>();
	shader->Initialize(desc);
	return shader;
}

void D3D12Device::WaitForIdle()
{
	DYNAMIC_ASSERT(m_device, "D3D12Device::WaitForIdle: device not initialized");

	D3D12Fence fence;
	fence.InitializeWithDevice(m_device.Get());

	// Create a temporary direct command queue just to issue a GPU signal
	ComRef<ID3D12CommandQueue> tmpQueue;
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type					   = D3D12_COMMAND_LIST_TYPE_DIRECT;
	ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&tmpQueue)));

	fence.GPUSignal(tmpQueue.Get());
	fence.WaitForValue(fence.GetNextValue() - 1);
}

#endif
