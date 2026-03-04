#pragma once

#include <Rendering/Renderer/SwapChain.h>

#ifdef WARP_WINDOWS

#include <d3d12.h>
#include <dxgi1_4.h>
#include <dxgi1_5.h>
#include <dxgi1_6.h>

class D3D12SwapChain : public SwapChain
{
public:
	// Called by D3D12Device::CreateSwapChain.
	// device is needed to create the RTV descriptor heap.
	void InitializeWithFactory(ID3D12Device* device, IDXGIFactory4* factory, ID3D12CommandQueue* queue,
							   const SwapChainDesc& desc);

	// SwapChain interface
	void Initialize(const SwapChainDesc& desc) override;
	void Present() override;
	void Resize(u32 width, u32 height) override;
	void* GetCurrentBackBuffer() const override;
	SwapChainFormat GetFormat() const override;
	u32 GetWidth() const override;
	u32 GetHeight() const override;
	void Cleanup() override;

	DescriptorHandle GetCurrentRTV() const override;
	void TransitionToRenderTarget(CommandList& cmd) override;
	void TransitionToPresent(CommandList& cmd) override;

	u32 GetCurrentBackBufferIndex() const;

private:
	void CreateRTVs(ID3D12Device* device);

	static DXGI_FORMAT ToDXGIFormat(SwapChainFormat format);

	ComRef<IDXGISwapChain4> m_swapChain;
	Vector<ComRef<ID3D12Resource>> m_backBuffers;
	Vector<D3D12_RESOURCE_STATES> m_backBufferStates;

	ComRef<ID3D12DescriptorHeap> m_rtvHeap;
	u32 m_rtvDescriptorSize = 0;

	u32 m_bufferCount			   = 2;
	u32 m_width					   = 0;
	u32 m_height				   = 0;
	DXGI_FORMAT m_format		   = DXGI_FORMAT_R8G8B8A8_UNORM;
	SwapChainFormat m_engineFormat = SwapChainFormat::RGBA8;
	bool m_vsync				   = false;
};

#endif
