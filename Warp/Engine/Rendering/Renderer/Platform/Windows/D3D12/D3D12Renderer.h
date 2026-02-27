#pragma once

#include <Rendering/Renderer/Renderer.h>

#ifdef WARP_WINDOWS

class D3D12Renderer : public Renderer
{
public:
	void Init(IWindow* window)           override;
	void Shutdown()                      override;
	void OnResize(u32 width, u32 height) override;
};

#endif
