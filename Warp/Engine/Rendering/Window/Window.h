#pragma once

#include <Common/CommonTypes.h>

class IWindow
{
public:

    virtual ~IWindow() {}

    virtual bool Create(String AppName, int Width, int Height) = 0;
    virtual void Destroy() = 0;
    virtual bool PumpMessages() = 0;

    int16 GetHeight() { return m_height; }
    int16 GetWidth() { return m_width; }

    virtual void* GetNativeHandle() const = 0;

protected:

    bool m_bAppPaused = false;
    bool m_bMinimized = false;
    bool m_bMaximized = false;
    bool m_bResizing = false;
    bool m_bFullScreenState = false;

    int16 m_width;
    int16 m_height;
};
