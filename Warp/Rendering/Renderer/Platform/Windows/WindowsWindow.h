#pragma once

#include <Common/CommonTypes.h>
#include <Rendering/Window/Window.h>

#ifdef WARP_WINDOWS

#include <Windows.h>

LRESULT CALLBACK
MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

class WindowsWindow : public IWindow
{
public:

    virtual bool Create(String AppName, int width, int height) final;
    virtual void Destroy();

private:

    HWND m_wndHnd;
    HINSTANCE m_appInst;
};

#endif