#pragma once

#include <Rendering/Renderer/Device.h>

#ifdef WARP_WINDOWS

class D3D12Device : public IDevice
{
public:
	const char* GetAPIName() override
	{
		return "D3D12";
	}

private:
};
#endif