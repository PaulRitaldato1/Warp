#include <EntryPoint/EntryPoint.h>
#include <Common/CommonTypes.h>
#include <Input/Input.h>
#include <Core/ECS/Components/TransformComponent.h>
#include <Core/ECS/Components/MeshComponent.h>
#include <Rendering/Cameras/FreeCam.h>

struct TempGame : public UserApplicationBase
{
	FreeCam camera;

	bool Initialize()
	{
		World& world = engine->GetWorld();

		Entity Helmet = world.CreateEntity<TransformComponent, MeshComponent>();
		world.GetComponent<MeshComponent>(Helmet).SetPath("Resources/DamagedHelmet/DamagedHelmet.gltf");

		Entity Avocado = world.CreateEntity<TransformComponent, MeshComponent>();
		world.GetComponent<MeshComponent>(Avocado).SetPath("Resources/Avocado/Avocado.gltf");
		TransformComponent& transform = world.GetComponent<TransformComponent>(Avocado);
		transform.Move({ 2.0f, 0.f, 0.f });
		transform.Scale(20.0f);

		Entity duplicateAvocado	  = world.DuplicateEntity(Avocado);
		TransformComponent& trans = world.GetComponent<TransformComponent>(duplicateAvocado);
		trans.Move({ 5.0f, 0.0f, 0.0f });
		trans.Scale(5.0f);

		const f32 aspect = static_cast<f32>(EngineInitDesc.WindowWidth) / static_cast<f32>(EngineInitDesc.WindowHeight);
		camera.Initialize(60.f, aspect);
		camera.SetPosition({ 0.f, 0.f, -5.f });
		engine->SetActiveCamera(&camera);

		return true;
	}

	bool Update(f32 DeltaTime)
	{
		World& world = engine->GetWorld();
		world.Each<TransformComponent>([&](Entity entity, TransformComponent& transform)
									   { transform.Rotate({ 0.f, 30.f * DeltaTime, 0.f }); });

		return true;
	}

	void OnResize(f32 DeltaTime)
	{
	}
};

bool HookEngineFromApp(UserApplicationBase** outDesc)
{
	(*outDesc) = new TempGame();

	(*outDesc)->EngineInitDesc.Name			= "Test Bed";
	(*outDesc)->EngineInitDesc.WindowWidth	= 1920;
	(*outDesc)->EngineInitDesc.WindowHeight = 1080;

	return true;
}
