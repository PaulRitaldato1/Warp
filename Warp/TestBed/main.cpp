#include <Debugging/Logging.h>
#include <Rendering/Renderer/Platform/Windows/WindowsWindow.h>

int main()
{
	Logging::Log("This is an Error", LOG_LEVEL::LOG_ERROR);
	Logging::Log("This is a Warning", LOG_LEVEL::LOG_WARNING);
	Logging::Log("This is a Debug", LOG_LEVEL::LOG_DEBUG);
	Logging::Log("This is an Info", LOG_LEVEL::LOG_INFO);

	IWindow* window = new WindowsWindow("WarpTest", 1920, 1080);	

	while(true)
	{
		window->PumpMessages();
	}
}