#pragma once

#ifdef WARP_BUILD_VK

#include <Rendering/Renderer/Renderer.h>

// ---------------------------------------------------------------------------
// VKRenderer — Vulkan implementation of the platform-specific Init / Shutdown
// / OnResize hooks.  All frame logic lives in the base Renderer class.
// ---------------------------------------------------------------------------

class VKRenderer : public Renderer
{
public:
	void Init(IWindow* window)           override;
	void Shutdown()                      override;
	void OnResize(u32 width, u32 height) override;

private:
	void CreateDepthBuffer(u32 width, u32 height);
};

#endif // WARP_BUILD_VK
