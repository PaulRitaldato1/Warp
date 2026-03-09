#include <EntryPoint/EntryPoint.h>
#include <Common/CommonTypes.h>
#include <Input/Input.h>
#include <Core/ECS/Components/TransformComponent.h>
#include <Core/ECS/Components/MeshComponent.h>

struct TempGame : public UserApplicationBase
{
	bool Initialize()
	{
		World& world = engine->GetWorld();

		Entity avocado = world.CreateEntity();
		world.AddComponent<TransformComponent>(avocado);
		world.AddComponent<MeshComponent>(avocado);
		world.GetComponent<MeshComponent>(avocado).SetPath("Resources/Avacado/Avocado.gltf");

		return true;
	}

	bool Update(f32 DeltaTime)
	{
		return true;
	}

	void OnResize(f32 DeltaTime)
	{
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