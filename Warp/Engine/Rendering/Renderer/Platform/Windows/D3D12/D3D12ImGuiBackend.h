#pragma once

#ifdef WARP_WINDOWS

#include <UI/ImGuiBackend.h>
#include <d3d12.h>

class D3D12ImGuiBackend : public ImGuiBackend
{
public:
	bool Init(IWindow* window, Device* device, CommandQueue* graphicsQueue, u32 framesInFlight) override;
	void Shutdown() override;
	void NewFrame() override;
	void Render(CommandList* commandList) override;

private:
	// Small dedicated SRV heap for ImGui's font atlas descriptor.
	// Separate from the per-frame ring heap so it persists across frame resets.
	ComRef<ID3D12DescriptorHeap> m_fontSrvHeap;
};

#endif
