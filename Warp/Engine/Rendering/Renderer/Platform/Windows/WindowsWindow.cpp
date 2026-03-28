#ifdef WARP_WINDOWS

#include "WindowsWindow.h"
#include <Debugging/Logging.h>
#include <cstdlib>
#include <Input/Input.h>

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <WindowsX.h>

// Forward declare the ImGui Win32 handler (defined in imgui_impl_win32.cpp).
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// Let ImGui process input, but only intercept events when the mouse
	// is NOT captured by the game (FreeCam). When captured, all mouse/keyboard
	// input goes to the game — ImGui only gets events when the cursor is free.
	WindowsWindow* window = reinterpret_cast<WindowsWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
	bool mouseCaptured = window && window->IsMouseCaptured();

	if (ImGui::GetCurrentContext() && !mouseCaptured)
	{
		ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
		const ImGuiIO& io = ImGui::GetIO();

		bool isMouseMsg    = (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) || msg == WM_INPUT;
		bool isKeyboardMsg = (msg >= WM_KEYFIRST && msg <= WM_KEYLAST);

		if ((isMouseMsg && io.WantCaptureMouse) || (isKeyboardMsg && io.WantCaptureKeyboard))
		{
			return 0;
		}
	}
	else if (ImGui::GetCurrentContext())
	{
		// Still feed non-mouse/keyboard messages to ImGui (e.g. WM_SIZE)
		// so it tracks window state correctly.
		bool isMouseMsg    = (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) || msg == WM_INPUT;
		bool isKeyboardMsg = (msg >= WM_KEYFIRST && msg <= WM_KEYLAST);
		if (!isMouseMsg && !isKeyboardMsg)
		{
			ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
		}
	}

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
		{
			WindowsWindow* wnd = reinterpret_cast<WindowsWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
			int16 newWidth  = static_cast<int16>(LOWORD(lParam));
			int16 newHeight = static_cast<int16>(HIWORD(lParam));

			if (wParam == SIZE_MINIMIZED)
			{
				if (wnd) wnd->SetMinimized(true);
			}
			else if (newWidth > 0 && newHeight > 0 && wnd)
			{
				wnd->SetMinimized(false);
				wnd->NotifyResize(newWidth, newHeight);
			}
			return 0;
		}

		// WM_ENTERSIZEMOVE is sent when the user grabs the resize bars.
		case WM_ENTERSIZEMOVE:
			return 0;

		// WM_EXITSIZEMOVE is sent when the user releases the resize bars.
		// Flag a resize so the swap chain updates once at the final size.
		case WM_EXITSIZEMOVE:
		{
			WindowsWindow* wnd = reinterpret_cast<WindowsWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
			if (wnd)
			{
				RECT clientRect;
				GetClientRect(hwnd, &clientRect);
				int16 newWidth  = static_cast<int16>(clientRect.right - clientRect.left);
				int16 newHeight = static_cast<int16>(clientRect.bottom - clientRect.top);
				wnd->NotifyResize(newWidth, newHeight);
			}
			return 0;
		}

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
		case WM_INPUT:
		{
			// Only forward mouse deltas when the mouse is captured — prevents
			// camera movement when the cursor is free for UI interaction.
			WindowsWindow* wnd = reinterpret_cast<WindowsWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
			if (!wnd || !wnd->IsMouseCaptured())
			{
				return 0;
			}

			// Read raw mouse data and broadcast relative (dx, dy) deltas.
			// This avoids the WM_MOUSEMOVE limitation where deltas go to zero
			// when the cursor hits the window edge.
			BYTE buffer[sizeof(RAWINPUT)];
			UINT size = sizeof(buffer);
			if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT,
			                    buffer, &size, sizeof(RAWINPUTHEADER)) != static_cast<UINT>(-1))
			{
				const RAWINPUT* raw = reinterpret_cast<const RAWINPUT*>(buffer);
				if (raw->header.dwType == RIM_TYPEMOUSE &&
				    !(raw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE))
				{
					g_InputEventManager.BroadcastMouseMove(
					    static_cast<int32>(raw->data.mouse.lLastX),
					    static_cast<int32>(raw->data.mouse.lLastY));
				}
			}
			return 0;
		}
		case WM_MOUSEMOVE:
			// Absolute position is handled by WM_INPUT above; nothing to do here.
			return 0;
		case WM_KEYDOWN:
			g_InputEventManager.BroadcastKey(static_cast<WarpKeyCode>(wParam), true);
			return 0;
		case WM_KEYUP:
			g_InputEventManager.BroadcastKey(static_cast<WarpKeyCode>(wParam), false);
			if (wParam == VK_TAB)
			{
				WindowsWindow* wnd = reinterpret_cast<WindowsWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
				if (wnd)
				{
					wnd->ToggleMouseCapture();
				}
			}
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
	// Set user data before ShowWindow so WM_SIZE messages during
	// the initial show can reach NotifyResize.
	SetWindowLongPtr(m_wndHnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
	ShowWindow(m_wndHnd, SW_SHOW);
	UpdateWindow(m_wndHnd);

	// Query the actual client area — on high-DPI displays the OS may have
	// scaled the window, so the real size can differ from what we requested.
	RECT actualClient;
	GetClientRect(m_wndHnd, &actualClient);
	m_width  = static_cast<int16>(actualClient.right - actualClient.left);
	m_height = static_cast<int16>(actualClient.bottom - actualClient.top);

	// Register for raw mouse input so mouse-move events deliver relative deltas
	// regardless of cursor position — prevents yaw from stopping at window edges.
	RAWINPUTDEVICE rid = {};
	rid.usUsagePage = 0x01; // HID_USAGE_PAGE_GENERIC
	rid.usUsage     = 0x02; // HID_USAGE_GENERIC_MOUSE
	rid.dwFlags     = 0;
	rid.hwndTarget  = m_wndHnd;
	RegisterRawInputDevices(&rid, 1, sizeof(rid));

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

void WindowsWindow::CaptureMouse()
{
	m_mouseCaptured = true;
	ShowCursor(FALSE);

	RECT rect;
	GetClientRect(m_wndHnd, &rect);
	MapWindowPoints(m_wndHnd, nullptr, reinterpret_cast<POINT*>(&rect), 2);
	ClipCursor(&rect);
}

void WindowsWindow::ReleaseMouse()
{
	m_mouseCaptured = false;
	ShowCursor(TRUE);
	ClipCursor(nullptr);
}

void WindowsWindow::ToggleMouseCapture()
{
	if (m_mouseCaptured)
	{
		ReleaseMouse();
	}
	else
	{
		CaptureMouse();
	}
}

#endif
