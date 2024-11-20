#pragma once

struct DeviceDesc
{
    bool bEnableDebugLayer = false;
    bool bEnableGPUValidationLayer = false;
};

struct PhysicalyDeviceInfo
{
    bool bSupportsHardwareRT = false;
    bool bSupportsFP16 = false;
    bool bSupportsMeshShaders = false;
    u32 SupportedMSSALevel = 0;
};

class IDevice
{
public:

};