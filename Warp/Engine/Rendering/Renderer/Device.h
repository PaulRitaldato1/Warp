#pragma once

#include <Common/CommonTypes.h>

class IBuffer;
struct BufferType;

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
};

class IDevice
{
public:
	IDevice()		   = default;
	virtual ~IDevice() = default;

	virtual URef<IBuffer> CreateBuffer(BufferType type, u32 size, bool bUseVRAM, const char* name) = 0;
	virtual const char* GetAPIName()															   = 0;

private:
	PhysicalDeviceInfo m_DeviceInfo;
};
