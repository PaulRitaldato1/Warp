#include <Rendering/Window/GlfwWindow.h>

#include <Debugging/Logging.h>
#include <Input/Input.h>

#include <imgui.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

// GLFW key -> WarpKeyCode. Letters, digits and space share their values with the
// Windows VK codes WarpKeyCode is built on, so they pass through. Everything else
// needs an entry.
static const HashMap<int, WarpKeyCode>& GetGlfwKeyMap()
{
	static const HashMap<int, WarpKeyCode> s_map = {
		{ GLFW_KEY_ESCAPE, KEY_ESCAPE },
		{ GLFW_KEY_ENTER, KEY_ENTER },
		{ GLFW_KEY_KP_ENTER, KEY_ENTER },
		{ GLFW_KEY_TAB, KEY_TAB },
		{ GLFW_KEY_BACKSPACE, KEY_BACKSPACE },
		{ GLFW_KEY_INSERT, KEY_INSERT },
		{ GLFW_KEY_DELETE, KEY_DELETE },
		{ GLFW_KEY_RIGHT, KEY_RIGHT },
		{ GLFW_KEY_LEFT, KEY_LEFT },
		{ GLFW_KEY_DOWN, KEY_DOWN },
		{ GLFW_KEY_UP, KEY_UP },
		{ GLFW_KEY_PAGE_UP, KEY_PRIOR },
		{ GLFW_KEY_PAGE_DOWN, KEY_NEXT },
		{ GLFW_KEY_HOME, KEY_HOME },
		{ GLFW_KEY_END, KEY_END },
		{ GLFW_KEY_CAPS_LOCK, KEY_CAPITAL },
		{ GLFW_KEY_SCROLL_LOCK, KEY_SCROLL },
		{ GLFW_KEY_NUM_LOCK, KEY_NUMLOCK },
		{ GLFW_KEY_PRINT_SCREEN, KEY_PRINT },
		{ GLFW_KEY_PAUSE, KEY_PAUSE },
		{ GLFW_KEY_F1, KEY_F1 },
		{ GLFW_KEY_F2, KEY_F2 },
		{ GLFW_KEY_F3, KEY_F3 },
		{ GLFW_KEY_F4, KEY_F4 },
		{ GLFW_KEY_F5, KEY_F5 },
		{ GLFW_KEY_F6, KEY_F6 },
		{ GLFW_KEY_F7, KEY_F7 },
		{ GLFW_KEY_F8, KEY_F8 },
		{ GLFW_KEY_F9, KEY_F9 },
		{ GLFW_KEY_F10, KEY_F10 },
		{ GLFW_KEY_F11, KEY_F11 },
		{ GLFW_KEY_F12, KEY_F12 },
		{ GLFW_KEY_KP_0, KEY_NUMPAD0 },
		{ GLFW_KEY_KP_1, KEY_NUMPAD1 },
		{ GLFW_KEY_KP_2, KEY_NUMPAD2 },
		{ GLFW_KEY_KP_3, KEY_NUMPAD3 },
		{ GLFW_KEY_KP_4, KEY_NUMPAD4 },
		{ GLFW_KEY_KP_5, KEY_NUMPAD5 },
		{ GLFW_KEY_KP_6, KEY_NUMPAD6 },
		{ GLFW_KEY_KP_7, KEY_NUMPAD7 },
		{ GLFW_KEY_KP_8, KEY_NUMPAD8 },
		{ GLFW_KEY_KP_9, KEY_NUMPAD9 },
		{ GLFW_KEY_KP_DECIMAL, KEY_DECIMAL },
		{ GLFW_KEY_KP_DIVIDE, KEY_DIVIDE },
		{ GLFW_KEY_KP_MULTIPLY, KEY_MULTIPLY },
		{ GLFW_KEY_KP_SUBTRACT, KEY_SUBTRACT },
		{ GLFW_KEY_KP_ADD, KEY_ADD },
		{ GLFW_KEY_LEFT_SHIFT, KEY_LSHIFT },
		{ GLFW_KEY_LEFT_CONTROL, KEY_LCONTROL },
		{ GLFW_KEY_LEFT_ALT, KEY_LMENU },
		{ GLFW_KEY_LEFT_SUPER, KEY_LWIN },
		{ GLFW_KEY_RIGHT_SHIFT, KEY_RSHIFT },
		{ GLFW_KEY_RIGHT_CONTROL, KEY_RCONTROL },
		{ GLFW_KEY_RIGHT_ALT, KEY_RMENU },
		{ GLFW_KEY_RIGHT_SUPER, KEY_RWIN },
		{ GLFW_KEY_MENU, KEY_APPS },
		{ GLFW_KEY_SEMICOLON, KEY_SEMICOLON },
		{ GLFW_KEY_EQUAL, KEY_PLUS },
		{ GLFW_KEY_COMMA, KEY_COMMA },
		{ GLFW_KEY_MINUS, KEY_MINUS },
		{ GLFW_KEY_PERIOD, KEY_PERIOD },
		{ GLFW_KEY_SLASH, KEY_SLASH },
		{ GLFW_KEY_GRAVE_ACCENT, KEY_GRAVE },
	};
	return s_map;
}

static WarpKeyCode TranslateGlfwKey(int key)
{
	if ((key >= GLFW_KEY_A && key <= GLFW_KEY_Z) || (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) || key == GLFW_KEY_SPACE)
	{
		return static_cast<WarpKeyCode>(key);
	}

	const HashMap<int, WarpKeyCode>& map = GetGlfwKeyMap();
	auto it								 = map.find(key);
	return it != map.end() ? it->second : KEYS_MAX_KEYS;
}

// ImGui installs its own GLFW callbacks and chains to these, so it sees every
// event first. The game only gets what ImGui does not want.
static bool ImGuiWantsKeyboard()
{
	return ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureKeyboard;
}

static bool ImGuiWantsMouse()
{
	return ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse;
}

// A text box is active, like the console input.
static bool ImGuiWantsTextInput()
{
	return ImGui::GetCurrentContext() && ImGui::GetIO().WantTextInput;
}

static GlfwWindow* FromGlfw(GLFWwindow* window)
{
	return static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
}

void GlfwWindow::KeyCallback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/)
{
	if (action == GLFW_REPEAT)
	{
		return;
	}

	GlfwWindow* self = FromGlfw(window);

	// Engine keys work regardless of ImGui, except while typing, where Tab
	// completes and Esc clears the text instead.
	if (action == GLFW_RELEASE && !ImGuiWantsTextInput())
	{
		if (key == GLFW_KEY_ESCAPE)
		{
			glfwSetWindowShouldClose(window, GLFW_TRUE);
		}
		else if (key == GLFW_KEY_TAB)
		{
			self->ToggleMouseCapture();
		}
	}

	if (!self->IsMouseCaptured() && ImGuiWantsKeyboard())
	{
		return;
	}

	const WarpKeyCode code = TranslateGlfwKey(key);
	if (code != KEYS_MAX_KEYS)
	{
		g_InputEventManager.BroadcastKey(code, action == GLFW_PRESS);
	}
}

void GlfwWindow::MouseButtonCallback(GLFWwindow* window, int button, int action, int /*mods*/)
{
	if (!FromGlfw(window)->IsMouseCaptured() && ImGuiWantsMouse())
	{
		return;
	}

	MouseCode code;
	switch (button)
	{
		case GLFW_MOUSE_BUTTON_LEFT:
			code = MouseCode::BUTTON_LEFT;
			break;
		case GLFW_MOUSE_BUTTON_RIGHT:
			code = MouseCode::BUTTON_RIGHT;
			break;
		case GLFW_MOUSE_BUTTON_MIDDLE:
			code = MouseCode::BUTTON_MIDDLE;
			break;
		default:
			return;
	}

	g_InputEventManager.BroadcastMouseButton(code, action == GLFW_PRESS);
}

void GlfwWindow::CursorPosCallback(GLFWwindow* window, double x, double y)
{
	GlfwWindow* self = FromGlfw(window);

	if (self->m_bFirstMouse)
	{
		self->m_lastMouseX	= x;
		self->m_lastMouseY	= y;
		self->m_bFirstMouse = false;
	}

	const double dx	  = x - self->m_lastMouseX;
	const double dy	  = y - self->m_lastMouseY;
	self->m_lastMouseX = x;
	self->m_lastMouseY = y;

	// Deltas only while captured, so a free cursor never turns the camera.
	if (self->IsMouseCaptured())
	{
		g_InputEventManager.BroadcastMouseMove(static_cast<int32>(dx), static_cast<int32>(dy));
	}
}

void GlfwWindow::FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
	// Framebuffer size is in pixels, which is what the swap chain needs. Window
	// size is in screen coordinates and differs under DPI scaling.
	FromGlfw(window)->NotifyResize(static_cast<int16>(width), static_cast<int16>(height));
}

void GlfwWindow::IconifyCallback(GLFWwindow* window, int iconified)
{
	FromGlfw(window)->SetMinimized(iconified == GLFW_TRUE);
}

bool GlfwWindow::Create(String appName, int width, int height)
{
	if (!glfwInit())
	{
		LOG_ERROR("GlfwWindow: glfwInit failed");
		return false;
	}

	// Neither backend wants a GL context.
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	m_window = glfwCreateWindow(width, height, appName.c_str(), nullptr, nullptr);
	if (!m_window)
	{
		LOG_ERROR("GlfwWindow: glfwCreateWindow failed");
		glfwTerminate();
		return false;
	}

	glfwSetWindowSizeLimits(m_window, 200, 200, GLFW_DONT_CARE, GLFW_DONT_CARE);

	glfwSetWindowUserPointer(m_window, this);
	glfwSetKeyCallback(m_window, KeyCallback);
	glfwSetMouseButtonCallback(m_window, MouseButtonCallback);
	glfwSetCursorPosCallback(m_window, CursorPosCallback);
	glfwSetFramebufferSizeCallback(m_window, FramebufferSizeCallback);
	glfwSetWindowIconifyCallback(m_window, IconifyCallback);

	// High DPI displays can make this differ from what was requested.
	int framebufferWidth  = 0;
	int framebufferHeight = 0;
	glfwGetFramebufferSize(m_window, &framebufferWidth, &framebufferHeight);
	m_width	 = static_cast<int16>(framebufferWidth);
	m_height = static_cast<int16>(framebufferHeight);

	LOG_DEBUG("GlfwWindow created: {}x{}", m_width, m_height);
	return true;
}

void GlfwWindow::Destroy()
{
	if (m_window)
	{
		glfwDestroyWindow(m_window);
		m_window = nullptr;
		glfwTerminate();
	}
}

bool GlfwWindow::PumpMessages()
{
	if (!m_window)
	{
		return false;
	}

	glfwPollEvents();
	return !glfwWindowShouldClose(m_window);
}

void GlfwWindow::CaptureMouse()
{
	if (!m_window)
	{
		return;
	}

	glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// Unaccelerated deltas, the same as the old WM_INPUT path.
	if (glfwRawMouseMotionSupported())
	{
		glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
	}

	// Keeps ImGui from reacting to a cursor it cannot see.
	if (ImGui::GetCurrentContext())
	{
		ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
	}

	m_bFirstMouse = true;
}

void GlfwWindow::ReleaseMouse()
{
	if (!m_window)
	{
		return;
	}

	glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

	if (ImGui::GetCurrentContext())
	{
		ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
	}
}

void GlfwWindow::ToggleMouseCapture()
{
	if (IsMouseCaptured())
	{
		ReleaseMouse();
	}
	else
	{
		CaptureMouse();
	}
}

bool GlfwWindow::IsMouseCaptured() const
{
	return m_window && glfwGetInputMode(m_window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
}
