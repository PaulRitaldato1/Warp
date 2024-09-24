#include <Debugging/Logging.h>
#include <Rendering/Renderer/Platform/Windows/WindowsWindow.h>

int main()
{

	DEBUG_LOG("Debug Log");
	INFO_LOG("Info Log");
	WARNING_LOG("Warning Log");
	ERROR_LOG("Error Log");

	IWindow* window = new WindowsWindow("WarpTest", 1920, 1080);	

	while(true)
	{
		if(!window->PumpMessages())
		{
			break;
		}
	}
}