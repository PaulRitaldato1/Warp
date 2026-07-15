#pragma once

#include <Rendering/Window/Window.h>

#ifdef WARP_LINUX

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

class LinuxWindow : public IWindow
{
public:
	LinuxWindow(String Name, int width, int height)
	{
		Create(Name, width, height);
	}

	~LinuxWindow()
	{
		Destroy();
	}

	virtual bool Create(String AppName, int width, int height) final;
	virtual void Destroy() final;
	virtual bool PumpMessages() final;

	void* GetNativeHandle() const override { return (void*)m_glfwWindow; }

	void CaptureMouse()       override;
	void ReleaseMouse()       override;
	void ToggleMouseCapture() override;
	bool IsMouseCaptured()    const override;

private:
	static void KeyCallback(GLFWwindow* win, int key, int scancode, int action, int mods);
	static void MouseButtonCallback(GLFWwindow* win, int button, int action, int mods);
	static void CursorPosCallback(GLFWwindow* win, double xpos, double ypos);
	static void WindowSizeCallback(GLFWwindow* win, int width, int height);
	static void WindowCloseCallback(GLFWwindow* win);

	GLFWwindow* m_glfwWindow = nullptr;
	double      m_lastMouseX = 0.0;
	double      m_lastMouseY = 0.0;
	bool        m_firstMouse = true;
};

#endif
