#include "WarpEngine.h"

#include <Debugging/Assert.h>
#include <Debugging/Logging.h>
#include <UserApplication.h>
#include <Rendering/Renderer/Renderer.h>
#include <Rendering/RenderBackend.h>
#include <Rendering/Resource/ResourceManager.h>
#include <Rendering/Window/Window.h>
#include <Rendering/Cameras/Camera.h>
#include <thread>

WarpEngine::WarpEngine(UserApplicationBase* App)
{
	FATAL_ASSERT(App != nullptr, "Engine was attempting to init with no UserApplicationBase");

	m_timer = GameTimer();
	Logger::Get().Init();
	LOG_DEBUG("Engine Init Started");

	m_app	= std::unique_ptr<UserApplicationBase>(App);
	m_world = std::make_unique<World>();

	m_backend  = RenderBackend::Create();
	m_window   = m_backend->MakeWindow(App->EngineInitDesc.Name, App->EngineInitDesc.WindowWidth,
									   App->EngineInitDesc.WindowHeight);
	m_renderer = m_backend->CreateRenderer();
	if (m_renderer)
	{
		m_renderer->Init(m_window.get());
		m_renderer->SetWorld(m_world.get());

		m_resourceManager = std::make_unique<ResourceManager>();
		m_resourceManager->Initialize(m_renderer->GetDevice(), m_renderer->GetThreadPool());
		m_renderer->SetResourceManager(m_resourceManager.get());
	}

	LOG_DEBUG("Render backend initialized ({})", m_renderer ? "renderer ready" : "no renderer");

	m_bIsRunning	= true;
	m_bIsSuspended	= false;
	m_bResizing		= false;
	m_bIsFullScreen = false;
	m_lastTime		= 0.0f;

	m_app->engine = this;
	FATAL_ASSERT(App->Initialize(), "Application failed to initialize");
}

WarpEngine::~WarpEngine()
{
	LOG_DEBUG("Shutting down");
	if (m_window && m_activeCamera)
	{
		m_window->ReleaseMouse();
	}
	if (m_world)
	{
		m_world->ShutdownSystems();
	}
	if (m_resourceManager)
	{
		m_resourceManager->Shutdown();
	}
	if (m_renderer)
	{
		m_renderer->Shutdown();
	}
	Logger::Get().Shutdown();
}

void WarpEngine::SetActiveCamera(Camera* camera)
{
	m_activeCamera = camera;
	if (m_window && m_activeCamera)
	{
		m_window->CaptureMouse();
	}
}

void WarpEngine::ClearActiveCamera()
{
	m_activeCamera = nullptr;
	if (m_window)
	{
		m_window->ReleaseMouse();
	}
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

		if (m_renderer && m_window->WasResized())
		{
			m_renderer->OnResize(static_cast<u32>(m_window->GetWidth()), static_cast<u32>(m_window->GetHeight()));
		}

		m_timer.Tick();

		if (!m_bIsSuspended)
		{
			f32 deltaTime = m_timer.DeltaTime();

			if (!m_app->Update(deltaTime))
			{
				LOG_ERROR("App update failed.");
				m_bIsRunning = false;
				break;
			}

			if (m_activeCamera)
			{
				m_activeCamera->Update(deltaTime);
				m_renderer->SetViewProjectionMatrix(m_activeCamera->GetViewProjectionMatrix());
			}

			m_world->UpdateSystems(deltaTime);

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
