#pragma once

#include <Common/CommonTypes.h>
#include <Core/GameTimer.h>
#include <Core/ECS/World.h>
#include <Input/Input.h>
#include <UI/EditorUI.h>

class IWindow;
class Renderer;
class RenderBackend;
class ResourceManager;
struct UserApplicationBase;

struct EngineDesc
{
	int16 WindowStartPosX;
	int16 WindowStartPosY;

	int16 WindowWidth;
	int16 WindowHeight;

	String Name;
};

class WARP_API WarpEngine
{
public:
	WarpEngine(UserApplicationBase* App);
	~WarpEngine();
	bool Run();

private:
	WarpEngine(WarpEngine const&)	  = delete;
	void operator=(WarpEngine const&) = delete;

	bool m_bIsRunning;
	bool m_bIsSuspended;
	bool m_bIsFullScreen;
	bool m_bResizing;

	f64 m_lastTime;
	GameTimer m_timer;

	URef<RenderBackend> m_backend;
	URef<IWindow> m_window;
	URef<Renderer> m_renderer;
	URef<World> m_world;
	URef<ResourceManager> m_resourceManager;

	URef<UserApplicationBase> m_app;
	EditorUI m_editorUI;

public:
	World& GetWorld()
	{
		return *m_world;
	}

	IWindow* GetWindow()
	{
		return m_window.get();
	}

	ResourceManager* GetResourceManager()
	{
		return m_resourceManager.get();
	}
};
