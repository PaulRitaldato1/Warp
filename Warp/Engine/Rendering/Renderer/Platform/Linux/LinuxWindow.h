#pragma once

#include <Rendering/Window/Window.h>

#ifdef WARP_LINUX

#include <X11/Xlib.h>

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

	void* GetNativeHandle()  const override { return (void*)(uintptr_t)m_window; }
	void* GetNativeDisplay() const override { return (void*)m_display; }

private:
	Display* m_display		  = nullptr;
	::Window  m_window		  = 0;
	Atom	  m_wmDeleteMessage = 0;
};

#endif
