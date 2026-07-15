#include "LinuxWindow.h"

#ifdef WARP_LINUX

#include <Debugging/Logging.h>
#include <Input/Input.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

// GLFW key -> WarpKeyCode. Letters (GLFW_KEY_A-Z = 65-90) and digits
// (GLFW_KEY_0-9 = 48-57) share the same values as Windows VK codes, so they
// are handled inline. Everything else needs an explicit entry.
static const HashMap<int, WarpKeyCode>& GetGLFWKeyMap()
{
	static HashMap<int, WarpKeyCode> s_map = {
		{ GLFW_KEY_ESCAPE,        KEY_ESCAPE    },
		{ GLFW_KEY_ENTER,         KEY_ENTER     },
		{ GLFW_KEY_KP_ENTER,      KEY_ENTER     },
		{ GLFW_KEY_TAB,           KEY_TAB       },
		{ GLFW_KEY_BACKSPACE,     KEY_BACKSPACE },
		{ GLFW_KEY_INSERT,        KEY_INSERT    },
		{ GLFW_KEY_DELETE,        KEY_DELETE    },
		{ GLFW_KEY_RIGHT,         KEY_RIGHT     },
		{ GLFW_KEY_LEFT,          KEY_LEFT      },
		{ GLFW_KEY_DOWN,          KEY_DOWN      },
		{ GLFW_KEY_UP,            KEY_UP        },
		{ GLFW_KEY_PAGE_UP,       KEY_PRIOR     },
		{ GLFW_KEY_PAGE_DOWN,     KEY_NEXT      },
		{ GLFW_KEY_HOME,          KEY_HOME      },
		{ GLFW_KEY_END,           KEY_END       },
		{ GLFW_KEY_CAPS_LOCK,     KEY_CAPITAL   },
		{ GLFW_KEY_SCROLL_LOCK,   KEY_SCROLL    },
		{ GLFW_KEY_NUM_LOCK,      KEY_NUMLOCK   },
		{ GLFW_KEY_PRINT_SCREEN,  KEY_PRINT     },
		{ GLFW_KEY_PAUSE,         KEY_PAUSE     },
		{ GLFW_KEY_F1,  KEY_F1  }, { GLFW_KEY_F2,  KEY_F2  }, { GLFW_KEY_F3,  KEY_F3  },
		{ GLFW_KEY_F4,  KEY_F4  }, { GLFW_KEY_F5,  KEY_F5  }, { GLFW_KEY_F6,  KEY_F6  },
		{ GLFW_KEY_F7,  KEY_F7  }, { GLFW_KEY_F8,  KEY_F8  }, { GLFW_KEY_F9,  KEY_F9  },
		{ GLFW_KEY_F10, KEY_F10 }, { GLFW_KEY_F11, KEY_F11 }, { GLFW_KEY_F12, KEY_F12 },
		{ GLFW_KEY_F13, KEY_F13 }, { GLFW_KEY_F14, KEY_F14 }, { GLFW_KEY_F15, KEY_F15 },
		{ GLFW_KEY_F16, KEY_F16 }, { GLFW_KEY_F17, KEY_F17 }, { GLFW_KEY_F18, KEY_F18 },
		{ GLFW_KEY_F19, KEY_F19 }, { GLFW_KEY_F20, KEY_F20 }, { GLFW_KEY_F21, KEY_F21 },
		{ GLFW_KEY_F22, KEY_F22 }, { GLFW_KEY_F23, KEY_F23 }, { GLFW_KEY_F24, KEY_F24 },
		{ GLFW_KEY_KP_0,        KEY_NUMPAD0  }, { GLFW_KEY_KP_1, KEY_NUMPAD1 },
		{ GLFW_KEY_KP_2,        KEY_NUMPAD2  }, { GLFW_KEY_KP_3, KEY_NUMPAD3 },
		{ GLFW_KEY_KP_4,        KEY_NUMPAD4  }, { GLFW_KEY_KP_5, KEY_NUMPAD5 },
		{ GLFW_KEY_KP_6,        KEY_NUMPAD6  }, { GLFW_KEY_KP_7, KEY_NUMPAD7 },
		{ GLFW_KEY_KP_8,        KEY_NUMPAD8  }, { GLFW_KEY_KP_9, KEY_NUMPAD9 },
		{ GLFW_KEY_KP_DECIMAL,  KEY_DECIMAL  },
		{ GLFW_KEY_KP_DIVIDE,   KEY_DIVIDE   },
		{ GLFW_KEY_KP_MULTIPLY, KEY_MULTIPLY },
		{ GLFW_KEY_KP_SUBTRACT, KEY_SUBTRACT },
		{ GLFW_KEY_KP_ADD,      KEY_ADD      },
		{ GLFW_KEY_LEFT_SHIFT,    KEY_LSHIFT   },
		{ GLFW_KEY_LEFT_CONTROL,  KEY_LCONTROL },
		{ GLFW_KEY_LEFT_ALT,      KEY_LMENU    },
		{ GLFW_KEY_LEFT_SUPER,    KEY_LWIN     },
		{ GLFW_KEY_RIGHT_SHIFT,   KEY_RSHIFT   },
		{ GLFW_KEY_RIGHT_CONTROL, KEY_RCONTROL },
		{ GLFW_KEY_RIGHT_ALT,     KEY_RMENU    },
		{ GLFW_KEY_RIGHT_SUPER,   KEY_RWIN     },
		{ GLFW_KEY_MENU,          KEY_APPS     },
		{ GLFW_KEY_SEMICOLON,     KEY_SEMICOLON },
		{ GLFW_KEY_EQUAL,         KEY_PLUS     },
		{ GLFW_KEY_COMMA,         KEY_COMMA    },
		{ GLFW_KEY_MINUS,         KEY_MINUS    },
		{ GLFW_KEY_PERIOD,        KEY_PERIOD   },
		{ GLFW_KEY_SLASH,         KEY_SLASH    },
		{ GLFW_KEY_GRAVE_ACCENT,  KEY_GRAVE    },
	};
	return s_map;
}

static WarpKeyCode TranslateGLFWKey(int key)
{
	if ((key >= GLFW_KEY_A && key <= GLFW_KEY_Z) ||
	    (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) ||
	    key == GLFW_KEY_SPACE)
	{
		return static_cast<WarpKeyCode>(key);
	}
	const auto& map = GetGLFWKeyMap();
	auto it         = map.find(key);
	return (it != map.end()) ? it->second : KEYS_MAX_KEYS;
}

// ---------------------------------------------------------------------------
// GLFW callbacks
// ---------------------------------------------------------------------------

void LinuxWindow::KeyCallback(GLFWwindow* win, int key, int /*scancode*/, int action, int /*mods*/)
{
	if (action == GLFW_REPEAT)
		return;

	auto* self = static_cast<LinuxWindow*>(glfwGetWindowUserPointer(win));

	if (action == GLFW_PRESS)
	{
		if (key == GLFW_KEY_ESCAPE)
		{
			glfwSetWindowShouldClose(win, GLFW_TRUE);
			return;
		}
		if (key == GLFW_KEY_TAB)
		{
			self->ToggleMouseCapture();
		}
	}

	WarpKeyCode code = TranslateGLFWKey(key);
	if (code != KEYS_MAX_KEYS)
	{
		g_InputEventManager.BroadcastKey(code, action == GLFW_PRESS);
	}
}

void LinuxWindow::MouseButtonCallback(GLFWwindow* /*win*/, int button, int action, int /*mods*/)
{
	MouseCode code;
	if      (button == GLFW_MOUSE_BUTTON_LEFT)   code = MouseCode::BUTTON_LEFT;
	else if (button == GLFW_MOUSE_BUTTON_RIGHT)  code = MouseCode::BUTTON_RIGHT;
	else if (button == GLFW_MOUSE_BUTTON_MIDDLE) code = MouseCode::BUTTON_MIDDLE;
	else return;

	g_InputEventManager.BroadcastMouseButton(code, action == GLFW_PRESS);
}

void LinuxWindow::CursorPosCallback(GLFWwindow* win, double xpos, double ypos)
{
	auto* self = static_cast<LinuxWindow*>(glfwGetWindowUserPointer(win));

	if (self->m_firstMouse)
	{
		self->m_lastMouseX = xpos;
		self->m_lastMouseY = ypos;
		self->m_firstMouse = false;
	}

	if (glfwGetInputMode(win, GLFW_CURSOR) == GLFW_CURSOR_DISABLED)
	{
		int32 dx = static_cast<int32>(xpos - self->m_lastMouseX);
		int32 dy = static_cast<int32>(ypos - self->m_lastMouseY);
		g_InputEventManager.BroadcastMouseMove(dx, dy);
	}
	else
	{
		g_InputEventManager.BroadcastMouseMove(static_cast<int32>(xpos), static_cast<int32>(ypos));
	}

	self->m_lastMouseX = xpos;
	self->m_lastMouseY = ypos;
}

void LinuxWindow::WindowSizeCallback(GLFWwindow* win, int width, int height)
{
	auto* self = static_cast<LinuxWindow*>(glfwGetWindowUserPointer(win));
	self->NotifyResize(static_cast<int16>(width), static_cast<int16>(height));
}

void LinuxWindow::WindowCloseCallback(GLFWwindow* win)
{
	glfwSetWindowShouldClose(win, GLFW_TRUE);
}

// ---------------------------------------------------------------------------

bool LinuxWindow::Create(String AppName, int width, int height)
{
	m_width  = static_cast<int16>(width);
	m_height = static_cast<int16>(height);

	if (!glfwInit())
	{
		LOG_ERROR("LinuxWindow: glfwInit failed");
		return false;
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE,  GLFW_TRUE);

	m_glfwWindow = glfwCreateWindow(width, height, AppName.c_str(), nullptr, nullptr);
	if (!m_glfwWindow)
	{
		LOG_ERROR("LinuxWindow: glfwCreateWindow failed");
		glfwTerminate();
		return false;
	}

	glfwSetWindowUserPointer(m_glfwWindow, this);
	glfwSetKeyCallback(m_glfwWindow,         KeyCallback);
	glfwSetMouseButtonCallback(m_glfwWindow, MouseButtonCallback);
	glfwSetCursorPosCallback(m_glfwWindow,   CursorPosCallback);
	glfwSetWindowSizeCallback(m_glfwWindow,  WindowSizeCallback);
	glfwSetWindowCloseCallback(m_glfwWindow, WindowCloseCallback);

	LOG_DEBUG("LinuxWindow created via GLFW");
	return true;
}

void LinuxWindow::Destroy()
{
	if (m_glfwWindow)
	{
		glfwDestroyWindow(m_glfwWindow);
		m_glfwWindow = nullptr;
	}
	glfwTerminate();
}

bool LinuxWindow::PumpMessages()
{
	if (!m_glfwWindow)
		return false;

	glfwPollEvents();
	return !glfwWindowShouldClose(m_glfwWindow);
}

void LinuxWindow::CaptureMouse()
{
	if (!m_glfwWindow)
		return;
	glfwSetInputMode(m_glfwWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	if (glfwRawMouseMotionSupported())
		glfwSetInputMode(m_glfwWindow, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
	m_firstMouse = true;
}

void LinuxWindow::ReleaseMouse()
{
	if (!m_glfwWindow)
		return;
	glfwSetInputMode(m_glfwWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

void LinuxWindow::ToggleMouseCapture()
{
	if (IsMouseCaptured())
		ReleaseMouse();
	else
		CaptureMouse();
}

bool LinuxWindow::IsMouseCaptured() const
{
	return m_glfwWindow &&
	       glfwGetInputMode(m_glfwWindow, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
}

#endif
