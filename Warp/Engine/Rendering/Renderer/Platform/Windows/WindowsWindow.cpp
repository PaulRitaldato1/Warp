#ifdef WARP_WINDOWS

#include "WindowsWindow.h"
#include <Debugging/Logging.h>
#include <cstdlib>
#include <Input/Input.h>


#include <WindowsX.h>

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		// WM_ACTIVATE is sent when the window is activated or deactivated.
		// We pause the game when the window is deactivated and unpause it
		// when it becomes active.
		case WM_ACTIVATE:
			if (LOWORD(wParam) == WA_INACTIVE)
			{
				// TODO: Send pause event

				// mAppPaused = true;
				// mTimer.Stop();
			}
			else
			{
				// TODO: Unpause
				// mAppPaused = false;
				// mTimer.Start();
			}
			return 0;

		// WM_SIZE is sent when the user resizes the window.
		case WM_SIZE:
			// TODO: Pause and Send RESIZE event

			// Save the new client area dimensions.
			// mClientWidth  = LOWORD(lParam);
			// mClientHeight = HIWORD(lParam);
			if (wParam == SIZE_MINIMIZED)
			{
				// mAppPaused = true;
				// mMinimized = true;
				// mMaximized = false;
			}
			else if (wParam == SIZE_MAXIMIZED)
			{
				// mAppPaused = false;
				// mMinimized = false;
				// mMaximized = true;
				// OnResize();
			}
			else if (wParam == SIZE_RESTORED)
			{

				// Restoring from minimized state?
				// if( mMinimized )
				// {
				//     mAppPaused = false;
				//     mMinimized = false;
				//     OnResize();
				// }

				// Restoring from maximized state?
				// else if( mMaximized )
				// {
				//     mAppPaused = false;
				//     mMaximized = false;
				//     OnResize();
				// }
				// else if( mResizing )
				// {
				//     // If user is dragging the resize bars, we do not resize
				//     // the buffers here because as the user continuously
				//     // drags the resize bars, a stream of WM_SIZE messages are
				//     // sent to the window, and it would be pointless (and slow)
				//     // to resize for each WM_SIZE message received from dragging
				//     // the resize bars.  So instead, we reset after the user is
				//     // done resizing the window and releases the resize bars, which
				//     // sends a WM_EXITSIZEMOVE message.
				// }
				// else // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
				// {
				//     OnResize();
				// }
			}
			return 0;

		// WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
		case WM_ENTERSIZEMOVE:
			// mAppPaused = true;
			// mResizing  = true;
			// mTimer.Stop();
			return 0;

		// WM_EXITSIZEMOVE is sent when the user releases the resize bars.
		// Here we reset everything based on the new window dimensions.
		case WM_EXITSIZEMOVE:
			// mAppPaused = false;
			// mResizing  = false;
			// mTimer.Start();
			// OnResize();
			return 0;

		// WM_DESTROY is sent when the window is being destroyed.
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;

		// The WM_MENUCHAR message is sent when a menu is active and the user presses
		// a key that does not correspond to any mnemonic or accelerator key.
		case WM_MENUCHAR:
			// Don't beep when we alt-enter.
			return MAKELRESULT(0, MNC_CLOSE);

		// Catch this message so to prevent the window from becoming too small.
		case WM_GETMINMAXINFO:
			((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
			((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
			return 0;

		case WM_LBUTTONDOWN:
			g_InputEventManager.BroadcastMouseButton(MouseCode::BUTTON_LEFT, true);
			return 0;
		case WM_MBUTTONDOWN:
			g_InputEventManager.BroadcastMouseButton(MouseCode::BUTTON_MIDDLE, true);
			return 0;
		case WM_RBUTTONDOWN:
			g_InputEventManager.BroadcastMouseButton(MouseCode::BUTTON_RIGHT, true);
			return 0;
		case WM_LBUTTONUP:
			g_InputEventManager.BroadcastMouseButton(MouseCode::BUTTON_LEFT, false);
			return 0;
		case WM_MBUTTONUP:
			g_InputEventManager.BroadcastMouseButton(MouseCode::BUTTON_MIDDLE, false);
			return 0;
		case WM_RBUTTONUP:
			g_InputEventManager.BroadcastMouseButton(MouseCode::BUTTON_RIGHT, false);
			return 0;
		case WM_MOUSEMOVE:
			g_InputEventManager.BroadcastMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			return 0;
		case WM_KEYDOWN:
			g_InputEventManager.BroadcastKey(static_cast<WarpKeyCode>(wParam), true);
			return 0;
		case WM_KEYUP:
			g_InputEventManager.BroadcastKey(static_cast<WarpKeyCode>(wParam), false);
			if (wParam == VK_ESCAPE)
			{
				PostQuitMessage(0);
			}
			return 0;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool WindowsWindow::Create(String AppName, int width, int height)
{
	m_width	 = width;
	m_height = height;

	m_appInst = GetModuleHandleA(0);

	WNDCLASS wc;
	wc.style		 = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc	 = MainWndProc;
	wc.cbClsExtra	 = 0;
	wc.cbWndExtra	 = 0;
	wc.hInstance	 = m_appInst;
	wc.hIcon		 = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor		 = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wc.lpszMenuName	 = 0;
	wc.lpszClassName = "WarpWindow";

	if (!RegisterClass(&wc))
	{
		MessageBox(0, "RegisterClass Failed in window Creation", 0, 0);
		return false;
	}

	RECT rect = { 0, 0, width, height };
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, false);
	int w = rect.right - rect.left;
	int h = rect.bottom - rect.top;

	m_wndHnd = CreateWindow("WarpWindow", AppName.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, w, h, 0,
							0, m_appInst, 0);

	if (!m_wndHnd)
	{
		MessageBox(0, "CreateWindow Failed.", 0, 0);
		return false;
	}
	ShowWindow(m_wndHnd, SW_SHOW);
	UpdateWindow(m_wndHnd);

	return true;
}

void WindowsWindow::Destroy()
{
	if (m_wndHnd)
	{
		DestroyWindow(m_wndHnd);
		m_wndHnd = 0;
		LOG_DEBUG("Destoryed Window");
	}
}

bool WindowsWindow::PumpMessages()
{
	MSG message;

	while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE))
	{
		if (message.message == WM_QUIT)
		{
			return false;
		}
		TranslateMessage(&message);
		DispatchMessageA(&message);
	}

	return true;
}

#endif
