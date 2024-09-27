#include "WarpEngine.h"

#include <Debugging/Logging.h>
#include <Debugging/Assert.h>
#include <UserApplication.h>
#include <Core/GameTimer.h>
#ifdef WARP_WINDOWS
    #include <Renderer/Platform/Windows/WindowsWindow.h>
#elif WARP_LINUX
#elif WARP_APPLE
#endif

WarpEngine::WarpEngine(UserApplicationBase* App)
{
    LOG_DEBUG("Engine Init Started")

    m_app = std::unique_ptr<UserApplicationBase>(App);

#ifdef WARP_WINDOWS
    m_window = std::make_unique<WindowsWindow>(App->EngineInitDesc.Name, App->EngineInitDesc.WindowWidth, App->EngineInitDesc.WindowHeight);
    LOG_DEBUG("Windows Window Initialized");
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
    m_bIsFullScreen = false;
    m_lastTime = 0.0f;

    FATAL_ASSERT(App->Initialize(), "Application failed to initialize");
}

bool WarpEngine::Run()
{
    LOG_DEBUG("Engine Run Started");
    while(m_bIsRunning)
    {
        if(!m_window->PumpMessages())
        {
            m_bIsRunning = false;
        }

        if(!m_bIsSuspended)
        {
            if(!m_app->Update(2.0f))
            {
                LOG_ERROR("App update failed.");
                m_bIsRunning = false;
                break;
            }
        }
    }
    LOG_DEBUG("Shutting down");
    m_bIsRunning = false;

    return true;
}