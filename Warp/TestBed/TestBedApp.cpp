#include <EntryPoint/EntryPoint.h>
#include <Common/CommonTypes.h>
#include <Input/Input.h>

struct TempGame : public UserApplicationBase
{
	bool Initialize()
	{
		return true;
	}

	bool Update(f32 DeltaTime)
	{
		if (DeltaTime > 0)
		{
		}
		// LOG_DEBUG("DeltaTime: " + std::to_string(DeltaTime));
		return true;
	}

	void OnResize(f32 DeltaTime)
	{
		if (DeltaTime > 1.0f)
		{
			// LOG_DEBUG("Whatever");
		}
	}
};

void wKeyDown()
{
	LOG_DEBUG("W Key Down");
}

bool HookEngineFromApp(UserApplicationBase** outDesc)
{
	(*outDesc) = new TempGame();

	(*outDesc)->EngineInitDesc.Name			= "Test Bed";
	(*outDesc)->EngineInitDesc.WindowWidth	= 1920;
	(*outDesc)->EngineInitDesc.WindowHeight = 1080;

	// g_InputEventManager.SubscribeToKeyUp(WarpKeyCode::KEY_W, wKeyUp);
	g_InputEventManager.SubscribeToKeyDown(WarpKeyCode::KEY_W, wKeyDown);

	return true;
}