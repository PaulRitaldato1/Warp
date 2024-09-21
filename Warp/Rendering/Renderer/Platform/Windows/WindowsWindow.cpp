#include "WindowsWindow.h"

#ifdef WARP_WINDOWS

bool WindowsWindow::Create(WString AppName, int width, int height)

{
    m_width = width;
    m_height = height;

    m_appInst = GetModuleHandleA(0);

    WNDCLASS wc;
    wc.style            = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc      = MainWndProc;
    wc.cbClsExtra       = 0;
    wc.cbWndExtra       = 0;
    wc.hInstance        = m_appInst;
    wc.hIcon            = LoadIcon(0, IDI_APPLICATION);
    wc.hCursor          = LoadCursor(0, IDC_ARROW);
    wc.hbrBackground    = (HBRUSH)GetStockObject(NULL_BRUSH);
    wc.lpszMenuName     = 0;
    wc.lpszClassName    = AppName.c_str();

    if(!RegisterClass(&wc))
    {
        MessageBox(0, L"RegisterClass Failed in window Creation", 0, 0);
        return false;
    }

    RECT rect = {0, 0, width, height};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, false);
    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;

    m_wndHnd = CreateWindow(AppName.c_str(), AppName.c_str(), WS_OVERLAPPEDWINDOW, 
        CW_USEDEFAULT, CW_USEDEFAULT, w, h, 0, 0, m_appInst, 0);

    if(!m_wndHnd)
    {
        MessageBox(0, L"CreateWindow Failed.", 0, 0);
        return false;
    }

    ShowWindow(m_wndHnd, SW_SHOW);
    UpdateWindow(m_wndHnd);

    return true;
}

#endif