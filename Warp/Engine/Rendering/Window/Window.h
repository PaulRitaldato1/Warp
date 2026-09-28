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

    // The GLFWwindow*. Backends derive what they need from it: an HWND for
    // D3D12, a surface for Vulkan.
    virtual void* GetNativeHandle() const = 0;

    // Called by platform message handlers when the window client area changes.
    void NotifyResize(int16 newWidth, int16 newHeight)
    {
        if (newWidth > 0 && newHeight > 0 && (newWidth != m_width || newHeight != m_height))
        {
            m_width    = newWidth;
            m_height   = newHeight;
            m_bResizing = true;
        }
    }

    void SetMinimized(bool minimized) { m_bMinimized = minimized; }

    // Hide the cursor and confine it to the window (e.g. for first-person camera).
    virtual void CaptureMouse() {}
    virtual void ReleaseMouse() {}
    virtual void ToggleMouseCapture() {}
    virtual bool IsMouseCaptured() const { return false; }

protected:

    bool m_bAppPaused = false;
    bool m_bMinimized = false;
    bool m_bMaximized = false;
    bool m_bResizing = false;
    bool m_bFullScreenState = false;

    int16 m_width;
    int16 m_height;
};
