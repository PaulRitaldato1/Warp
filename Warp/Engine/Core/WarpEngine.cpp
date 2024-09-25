#include "WarpEngine.h"

#include <Debugging/Logging.h>

#ifdef WARP_WINDOWS
    #include <Renderer/Platform/Windows/WindowsWindow.h>
#elif WARP_LINUX
#elif WARP_APPLE
#endif

/// @brief Init for the engine. Ugly platform detection will happen here for init but otherwise through interfaces
/// @param desc 
WarpEngine::WarpEngine(EngineDesc& desc)
{
    LOG_DEBUG("App Init");
    
    //do platform and engine init
#ifdef WARP_WINDOWS
    LOG_DEBUG("Windows Window Initializing");
    m_window = std::make_unique<WindowsWindow>(desc.Name, desc.WindowWidth, desc.WindowHeight);
#elif WARP_LINUX
    LOG_DEBUG("Linux Window Initializing");
#elif WARP_APPLE
    LOG_DEBUG("Apple Window Initializing");
#endif

    m_bIsRunning = true;
    m_bIsSuspended = false;
    m_bMaximized = false;
    m_bMinimized = false;
    m_bResizing = false;
}

bool WarpEngine::Run()
{
    while(m_bIsRunning)
    {
        if(!m_window->PumpMessages())
        {
            m_bIsRunning = false;
        }
    }

    m_bIsRunning = false;

    return true;
}