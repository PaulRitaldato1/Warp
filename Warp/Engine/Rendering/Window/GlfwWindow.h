#pragma once

#include <Rendering/Window/Window.h>

struct GLFWwindow;

// One window implementation for every platform and both graphics APIs. D3D12
// gets its HWND from glfwGetWin32Window, Vulkan its surface from
// glfwCreateWindowSurface, and ImGui uses one platform backend for all of them.
class GlfwWindow : public IWindow
{
public:
	GlfwWindow(String name, int width, int height)
	{
		Create(name, width, height);
	}

	~GlfwWindow() override
	{
		Destroy();
	}

	bool Create(String appName, int width, int height) final;
	void Destroy() final;
	bool PumpMessages() final;

	// The GLFWwindow*, which is what both backends and ImGui take.
	void* GetNativeHandle() const override
	{
		return m_window;
	}

	void CaptureMouse() override;
	void ReleaseMouse() override;
	void ToggleMouseCapture() override;
	bool IsMouseCaptured() const override;

private:
	static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
	static void CursorPosCallback(GLFWwindow* window, double x, double y);
	static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
	static void IconifyCallback(GLFWwindow* window, int iconified);

	GLFWwindow* m_window = nullptr;

	// Last cursor position, for turning GLFW's absolute positions into deltas.
	double m_lastMouseX = 0.0;
	double m_lastMouseY = 0.0;
	bool m_bFirstMouse	= true;
};
