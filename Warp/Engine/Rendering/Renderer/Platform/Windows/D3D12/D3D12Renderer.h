#pragma once

#include <Rendering/Renderer/Renderer.h>

#ifdef WARP_WINDOWS

#include <d3d12.h>
#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12DescriptorHeap.h>

class D3D12Renderer : public Renderer
{
public:
	void Init(IWindow* window)           override;
	void Shutdown()                      override;
	void OnResize(u32 width, u32 height) override;

private:
	void CreateDepthBuffer(u32 width, u32 height);

	// DSV heap for the main depth buffer (1 slot, non-shader-visible).
	ComRef<ID3D12DescriptorHeap> m_dsvHeap;

	// Shader-visible CBV/SRV/UAV heap for per-frame dynamic descriptors.
	// Reset at Begin() for each graphics command list.
	D3D12DescriptorHeap m_srvHeap;
};

#endif
