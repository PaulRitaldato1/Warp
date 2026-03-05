#pragma once

#include <Rendering/Renderer/Texture.h>

#ifdef WARP_WINDOWS

#include <d3d12.h>

class D3D12Texture : public Texture
{
public:
	// Device-specific init — called by D3D12Device::CreateTexture().
	void InitializeWithDevice(ID3D12Device* device, const TextureDesc& desc);

	// Texture interface
	void  Init(const TextureDesc& desc) override; // no-op (use InitializeWithDevice)
	void  Cleanup() override;

	void*         GetNativeHandle() const override { return m_resource.Get(); }
	u32           GetWidth()        const override { return m_desc.width; }
	u32           GetHeight()       const override { return m_desc.height; }
	u32           GetDepth()        const override { return m_desc.depth; }
	u32           GetMipLevels()    const override { return m_desc.mipLevels; }
	TextureFormat GetFormat()       const override { return m_desc.format; }

	DescriptorHandle GetRTV() const override { return m_rtvHandle; }
	DescriptorHandle GetSRV() const override { return m_srvHandle; }
	DescriptorHandle GetDSV() const override { return m_dsvHandle; }

	// D3D12-specific: raw resource pointer and current barrier state.
	ID3D12Resource*       GetNativeResource() const  { return m_resource.Get(); }
	D3D12_RESOURCE_STATES GetCurrentState()   const  { return m_currentState; }
	void                  SetCurrentState(D3D12_RESOURCE_STATES s) { m_currentState = s; }

private:
	ComRef<ID3D12Resource>      m_resource;
	TextureDesc                 m_desc;
	D3D12_RESOURCE_STATES       m_currentState = D3D12_RESOURCE_STATE_COMMON;

	// Per-usage CPU descriptor heaps (one descriptor each).
	ComRef<ID3D12DescriptorHeap> m_rtvHeap;
	ComRef<ID3D12DescriptorHeap> m_dsvHeap;
	ComRef<ID3D12DescriptorHeap> m_srvHeap;

	DescriptorHandle m_rtvHandle;
	DescriptorHandle m_srvHandle;
	DescriptorHandle m_dsvHandle;
};

#endif
