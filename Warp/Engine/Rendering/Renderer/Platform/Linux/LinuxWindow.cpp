#include "LinuxWindow.h"

#ifdef WARP_LINUX

#include <Debugging/Logging.h>
#include <Input/Input.h>
#include <X11/keysym.h>

// X11 KeySym -> engine WarpKeyCode mapping
static const HashMap<KeySym, WarpKeyCode>& GetKeysymMap()
{
	static HashMap<KeySym, WarpKeyCode> s_map = {
		// Special keys
		{ XK_BackSpace,	 KEY_BACKSPACE },
		{ XK_Return,	 KEY_ENTER },
		{ XK_Tab,		 KEY_TAB },
		{ XK_Shift_L,	 KEY_LSHIFT },
		{ XK_Shift_R,	 KEY_RSHIFT },
		{ XK_Control_L,	 KEY_LCONTROL },
		{ XK_Control_R,	 KEY_RCONTROL },
		{ XK_Pause,		 KEY_PAUSE },
		{ XK_Caps_Lock,	 KEY_CAPITAL },
		{ XK_Escape,	 KEY_ESCAPE },
		{ XK_space,		 KEY_SPACE },
		{ XK_Prior,		 KEY_PRIOR },
		{ XK_Next,		 KEY_NEXT },
		{ XK_End,		 KEY_END },
		{ XK_Home,		 KEY_HOME },
		{ XK_Left,		 KEY_LEFT },
		{ XK_Up,		 KEY_UP },
		{ XK_Right,		 KEY_RIGHT },
		{ XK_Down,		 KEY_DOWN },
		{ XK_Select,	 KEY_SELECT },
		{ XK_Print,		 KEY_PRINT },
		{ XK_Execute,	 KEY_EXE },
		{ XK_Insert,	 KEY_INSERT },
		{ XK_Delete,	 KEY_DELETE },
		{ XK_Help,		 KEY_HELP },
		{ XK_Super_L,	 KEY_LWIN },
		{ XK_Super_R,	 KEY_RWIN },
		{ XK_Menu,		 KEY_APPS },
		{ XK_Alt_L,		 KEY_LMENU },
		{ XK_Alt_R,		 KEY_RMENU },
		// Alphabetical (base/lowercase keysyms)
		{ XK_a, KEY_A }, { XK_b, KEY_B }, { XK_c, KEY_C }, { XK_d, KEY_D },
		{ XK_e, KEY_E }, { XK_f, KEY_F }, { XK_g, KEY_G }, { XK_h, KEY_H },
		{ XK_i, KEY_I }, { XK_j, KEY_J }, { XK_k, KEY_K }, { XK_l, KEY_L },
		{ XK_m, KEY_M }, { XK_n, KEY_N }, { XK_o, KEY_O }, { XK_p, KEY_P },
		{ XK_q, KEY_Q }, { XK_r, KEY_R }, { XK_s, KEY_S }, { XK_t, KEY_T },
		{ XK_u, KEY_U }, { XK_v, KEY_V }, { XK_w, KEY_W }, { XK_x, KEY_X },
		{ XK_y, KEY_Y }, { XK_z, KEY_Z },
		// Numpad
		{ XK_KP_0,		   KEY_NUMPAD0 }, { XK_KP_1, KEY_NUMPAD1 },
		{ XK_KP_2,		   KEY_NUMPAD2 }, { XK_KP_3, KEY_NUMPAD3 },
		{ XK_KP_4,		   KEY_NUMPAD4 }, { XK_KP_5, KEY_NUMPAD5 },
		{ XK_KP_6,		   KEY_NUMPAD6 }, { XK_KP_7, KEY_NUMPAD7 },
		{ XK_KP_8,		   KEY_NUMPAD8 }, { XK_KP_9, KEY_NUMPAD9 },
		{ XK_KP_Multiply,  KEY_MULTIPLY },
		{ XK_KP_Add,	   KEY_ADD },
		{ XK_KP_Separator, KEY_SEPARATOR },
		{ XK_KP_Subtract,  KEY_SUBTRACT },
		{ XK_KP_Decimal,   KEY_DECIMAL },
		{ XK_KP_Divide,	   KEY_DIVIDE },
		{ XK_KP_Equal,	   KEY_NUMPAD_EQUAL },
		// Function keys
		{ XK_F1,  KEY_F1  }, { XK_F2,  KEY_F2  }, { XK_F3,  KEY_F3  },
		{ XK_F4,  KEY_F4  }, { XK_F5,  KEY_F5  }, { XK_F6,  KEY_F6  },
		{ XK_F7,  KEY_F7  }, { XK_F8,  KEY_F8  }, { XK_F9,  KEY_F9  },
		{ XK_F10, KEY_F10 }, { XK_F11, KEY_F11 }, { XK_F12, KEY_F12 },
		{ XK_F13, KEY_F13 }, { XK_F14, KEY_F14 }, { XK_F15, KEY_F15 },
		{ XK_F16, KEY_F16 }, { XK_F17, KEY_F17 }, { XK_F18, KEY_F18 },
		{ XK_F19, KEY_F19 }, { XK_F20, KEY_F20 }, { XK_F21, KEY_F21 },
		{ XK_F22, KEY_F22 }, { XK_F23, KEY_F23 }, { XK_F24, KEY_F24 },
		// Lock keys
		{ XK_Num_Lock,	  KEY_NUMLOCK },
		{ XK_Scroll_Lock, KEY_SCROLL },
		// Punctuation
		{ XK_semicolon, KEY_SEMICOLON },
		{ XK_equal,		KEY_PLUS },
		{ XK_comma,		KEY_COMMA },
		{ XK_minus,		KEY_MINUS },
		{ XK_period,	KEY_PERIOD },
		{ XK_slash,		KEY_SLASH },
		{ XK_grave,		KEY_GRAVE },
	};
	return s_map;
}

static WarpKeyCode TranslateKeySym(KeySym sym)
{
	const auto& map = GetKeysymMap();
	auto it			= map.find(sym);
	return (it != map.end()) ? it->second : KEYS_MAX_KEYS;
}

bool LinuxWindow::Create(String AppName, int width, int height)
{
	m_width	 = static_cast<int16>(width);
	m_height = static_cast<int16>(height);

	m_display = XOpenDisplay(nullptr);
	if (!m_display)
	{
		LOG_ERROR("LinuxWindow: XOpenDisplay failed - is DISPLAY set?");
		return false;
	}

	int screen	  = DefaultScreen(m_display);
	::Window root = RootWindow(m_display, screen);

	m_window = XCreateSimpleWindow(
		m_display, root,
		0, 0,
		static_cast<unsigned>(width), static_cast<unsigned>(height),
		1,
		BlackPixel(m_display, screen),
		BlackPixel(m_display, screen));

	if (!m_window)
	{
		LOG_ERROR("LinuxWindow: XCreateSimpleWindow failed");
		XCloseDisplay(m_display);
		m_display = nullptr;
		return false;
	}

	XSelectInput(m_display, m_window,
		KeyPressMask | KeyReleaseMask |
		ButtonPressMask | ButtonReleaseMask |
		PointerMotionMask | StructureNotifyMask);

	m_wmDeleteMessage = XInternAtom(m_display, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(m_display, m_window, &m_wmDeleteMessage, 1);

	XStoreName(m_display, m_window, AppName.c_str());
	XMapWindow(m_display, m_window);
	XFlush(m_display);

	LOG_DEBUG("Linux Window Created");
	return true;
}

void LinuxWindow::Destroy()
{
	if (m_display && m_window)
	{
		XDestroyWindow(m_display, m_window);
		m_window = 0;
		LOG_DEBUG("Destroyed Window");
	}
	if (m_display)
	{
		XCloseDisplay(m_display);
		m_display = nullptr;
	}
}

bool LinuxWindow::PumpMessages()
{
	if (!m_display)
	{
		return false;
	}

	while (XPending(m_display) > 0)
	{
		XEvent event;
		XNextEvent(m_display, &event);

		switch (event.type)
		{
			case KeyPress:
			{
				KeySym sym		 = XLookupKeysym(&event.xkey, 0);
				WarpKeyCode code = TranslateKeySym(sym);
				if (code != KEYS_MAX_KEYS)
				{
					g_InputEventManager.BroadcastKey(code, true);
				}
				if (sym == XK_Escape)
				{
					return false;
				}
				break;
			}
			case KeyRelease:
			{
				KeySym sym		 = XLookupKeysym(&event.xkey, 0);
				WarpKeyCode code = TranslateKeySym(sym);
				if (code != KEYS_MAX_KEYS)
				{
					g_InputEventManager.BroadcastKey(code, false);
				}
				break;
			}
			case ButtonPress:
			{
				if (event.xbutton.button == Button1)
				{
					g_InputEventManager.BroadcastMouseButton(MouseCode::BUTTON_LEFT, true);
				}
				else if (event.xbutton.button == Button2)
				{
					g_InputEventManager.BroadcastMouseButton(MouseCode::BUTTON_MIDDLE, true);
				}
				else if (event.xbutton.button == Button3)
				{
					g_InputEventManager.BroadcastMouseButton(MouseCode::BUTTON_RIGHT, true);
				}
				break;
			}
			case ButtonRelease:
			{
				if (event.xbutton.button == Button1)
				{
					g_InputEventManager.BroadcastMouseButton(MouseCode::BUTTON_LEFT, false);
				}
				else if (event.xbutton.button == Button2)
				{
					g_InputEventManager.BroadcastMouseButton(MouseCode::BUTTON_MIDDLE, false);
				}
				else if (event.xbutton.button == Button3)
				{
					g_InputEventManager.BroadcastMouseButton(MouseCode::BUTTON_RIGHT, false);
				}
				break;
			}
			case MotionNotify:
			{
				if (m_invisibleCursor != 0)
				{
					// When captured, warp to centre each frame for unbounded delta movement.
					const int cx = m_width / 2;
					const int cy = m_height / 2;
					const int x  = event.xmotion.x;
					const int y  = event.xmotion.y;

					// Skip warp-generated events to avoid feedback loop.
					if (x == cx && y == cy)
					{
						break;
					}

					// Broadcast relative delta directly — matches Windows WM_INPUT behaviour.
					g_InputEventManager.BroadcastMouseMove(x - cx, y - cy);

					// Warp cursor back to centre for the next frame.
					XWarpPointer(m_display, None, m_window, 0, 0, 0, 0, cx, cy);
					XFlush(m_display);
				}
				else
				{
					g_InputEventManager.BroadcastMouseMove(event.xmotion.x, event.xmotion.y);
				}
				break;
			}
			case ClientMessage:
			{
				if (static_cast<Atom>(event.xclient.data.l[0]) == m_wmDeleteMessage)
				{
					return false;
				}
				break;
			}
			case ConfigureNotify:
			{
				const int newW = event.xconfigure.width;
				const int newH = event.xconfigure.height;
				if (newW != m_width || newH != m_height)
				{
					m_width     = static_cast<int16>(newW);
					m_height    = static_cast<int16>(newH);
					m_bResizing = true;
				}
				break;
			}
			case DestroyNotify:
			{
				return false;
			}
			default:
				break;
		}
	}

	return true;
}

void LinuxWindow::CaptureMouse()
{
	if (!m_display || !m_window)
	{
		return;
	}

	// Create a 1x1 invisible cursor.
	static char data[1] = { 0 };
	Pixmap blank         = XCreateBitmapFromData(m_display, m_window, data, 1, 1);
	XColor dummy         = {};
	m_invisibleCursor    = XCreatePixmapCursor(m_display, blank, blank, &dummy, &dummy, 0, 0);
	XFreePixmap(m_display, blank);

	XDefineCursor(m_display, m_window, m_invisibleCursor);

	// Confine pointer to this window.
	XGrabPointer(m_display, m_window, True,
		ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
		GrabModeAsync, GrabModeAsync, m_window, m_invisibleCursor, CurrentTime);

	// Warp to centre so the first delta starts from a known position.
	XWarpPointer(m_display, None, m_window, 0, 0, 0, 0, m_width / 2, m_height / 2);
	XFlush(m_display);
}

void LinuxWindow::ReleaseMouse()
{
	if (!m_display)
	{
		return;
	}

	XUngrabPointer(m_display, CurrentTime);
	XUndefineCursor(m_display, m_window);

	if (m_invisibleCursor != 0)
	{
		XFreeCursor(m_display, m_invisibleCursor);
		m_invisibleCursor = 0;
	}

	XFlush(m_display);
}

#endif
