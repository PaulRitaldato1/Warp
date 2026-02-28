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
    int16 GetWidth()  { return m_width;  }

    // Returns true (once) if the window was resized since the last call.
    // Resets the flag so the caller can respond exactly once per resize.
    bool WasResized()
    {
        bool r      = m_bResizing;
        m_bResizing = false;
        return r;
    }

    virtual void* GetNativeHandle()   const = 0;

    // Returns the platform display/connection handle needed for Vulkan surface creation.
    // D3D12 / non-Vulkan backends return nullptr.
    // Linux X11: returns Display*   macOS: returns NSWindow* (future)
    virtual void* GetNativeDisplay() const { return nullptr; }

protected:

    bool m_bAppPaused = false;
    bool m_bMinimized = false;
    bool m_bMaximized = false;
    bool m_bResizing = false;
    bool m_bFullScreenState = false;

    int16 m_width;
    int16 m_height;
};
