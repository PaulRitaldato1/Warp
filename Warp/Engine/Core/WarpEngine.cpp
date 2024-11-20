#include "WarpEngine.h"

#include <Debugging/Assert.h>
#include <Debugging/Logging.h>
#include <UserApplication.h>

#ifdef WARP_WINDOWS
#include <Renderer/Platform/Windows/WindowsWindow.h>
#elif WARP_LINUX
#elif WARP_APPLE
#endif

WarpEngine::WarpEngine(UserApplicationBase* App)
{
	FATAL_ASSERT(App != nullptr, "Engine was attempting to init with no UserApplicationBase or "
								 "WarpEngine::Instance() was called before WarpEngine::Init()");

	m_timer = GameTimer();
	LOG_DEBUG("Engine Init Started")

	m_app = std::unique_ptr<UserApplicationBase>(App);

#ifdef WARP_WINDOWS
	m_window = std::make_unique<WindowsWindow>(App->EngineInitDesc.Name, App->EngineInitDesc.WindowWidth,
											   App->EngineInitDesc.WindowHeight);
	LOG_DEBUG("Windows Window Initialized");
#elif WARP_LINUX
	LOG_DEBUG("Linux Window Initializing");
#elif WARP_APPLE
	LOG_DEBUG("Apple Window Initializing");
#endif

	m_bIsRunning	= true;
	m_bIsSuspended	= false;
	m_bResizing		= false;
	m_bIsFullScreen = false;
	m_lastTime		= 0.0f;

	FATAL_ASSERT(App->Initialize(), "Application failed to initialize");
}

WarpEngine::~WarpEngine()
{
	LOG_DEBUG("Shutting down");
}

bool WarpEngine::Run()
{
	LOG_DEBUG("Engine Run Started");

	m_timer.Reset();

	while (m_bIsRunning)
	{
		if (!m_window->PumpMessages())
		{
			m_bIsRunning = false;
		}

		m_timer.Tick();

		if (!m_bIsSuspended)
		{
			// LOG_INFO("DeltaTime: " + std::to_string(m_timer.DeltaTime()));
			// calc frame stats
			if (!m_app->Update(m_timer.DeltaTime()))
			{
				LOG_ERROR("App update failed.");
				m_bIsRunning = false;
				break;
			}
			// render
		}
		else
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}
	m_bIsRunning = false;

	return true;
}
