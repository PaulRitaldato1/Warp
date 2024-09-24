#pragma once

#include <Rendering/Window/Window.h>

#ifdef WARP_WINDOWS

#include <Windows.h>

LRESULT CALLBACK
MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

class WARP_API WindowsWindow : public IWindow
{
public:

    WindowsWindow(String Name, int width, int height) { Create(Name, width, height); }
    ~WindowsWindow() { Destroy(); }

    virtual bool Create(String AppName, int width, int height) final;
    virtual void Destroy() final;

    virtual bool PumpMessages() final;

private:

    HWND m_wndHnd;
    HINSTANCE m_appInst;
};

#endif