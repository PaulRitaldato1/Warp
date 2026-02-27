#include "WarpEngine.h"

#include <Debugging/Assert.h>
#include <Debugging/Logging.h>
#include <UserApplication.h>
#include <Rendering/Renderer/Renderer.h>
#include <thread>

WarpEngine::WarpEngine(UserApplicationBase* App)
{
	FATAL_ASSERT(App != nullptr, "Engine was attempting to init with no UserApplicationBase");

	m_timer = GameTimer();
	LOG_DEBUG("Engine Init Started")

	m_app = std::unique_ptr<UserApplicationBase>(App);

	m_backend  = RenderBackend::Create();
	m_window   = m_backend->CreateWindow(App->EngineInitDesc.Name,
										  App->EngineInitDesc.WindowWidth,
										  App->EngineInitDesc.WindowHeight);
	m_renderer = m_backend->CreateRenderer();
	if (m_renderer)
	{
		m_renderer->Init(m_window.get());
	}

	LOG_DEBUG("Render backend initialized (" + String(m_renderer ? "renderer ready" : "no renderer") + ")");

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
	if (m_renderer) { m_renderer->Shutdown(); }
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
			if (!m_app->Update(m_timer.DeltaTime()))
			{
				LOG_ERROR("App update failed.");
				m_bIsRunning = false;
				break;
			}

			if (m_renderer)
			{
				m_renderer->BeginFrame();
				m_renderer->Draw();
				m_renderer->EndFrame();
			}
		}
		else
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}
	m_bIsRunning = false;

	return true;
}
