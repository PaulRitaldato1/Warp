#include <EntryPoint/EntryPoint.h>
#include <Common/CommonTypes.h>
#include <Input/Input.h>
#include <Core/ECS/Components/TransformComponent.h>
#include <Core/ECS/Components/MeshComponent.h>
#include <Core/ECS/Components/CameraComponent.h>
#include <Core/ECS/Systems/FreeCamSystem.h>

struct TempGame : public UserApplicationBase
{
	bool Initialize()
	{
		World& world = engine->GetWorld();

		Entity helmet = world.CreateEntity<TransformComponent, MeshComponent>();
		world.GetComponent<MeshComponent>(helmet).SetPath("Resources/DamagedHelmet/DamagedHelmet.gltf");

		Entity avocado = world.CreateEntity<TransformComponent, MeshComponent>();
		world.GetComponent<MeshComponent>(avocado).SetPath("Resources/Avocado/Avocado.gltf");
		TransformComponent& avocadoTransform = world.GetComponent<TransformComponent>(avocado);
		avocadoTransform.Move({ 2.0f, 0.f, 0.f });
		avocadoTransform.Scale(20.0f);

		Entity duplicateAvocado = world.DuplicateEntity(avocado);
		TransformComponent& duplicateTransform = world.GetComponent<TransformComponent>(duplicateAvocado);
		duplicateTransform.Move({ 5.0f, 0.0f, 0.0f });
		duplicateTransform.Scale(5.0f);

		// Create camera entity with FreeCam controls.
		const f32 aspect = static_cast<f32>(EngineInitDesc.WindowWidth) / static_cast<f32>(EngineInitDesc.WindowHeight);

		Entity camera = world.CreateEntity<TransformComponent, CameraComponent>();
		world.GetComponent<TransformComponent>(camera).Move({ 0.f, 0.f, -5.f });
		world.GetComponent<CameraComponent>(camera).SetAspect(aspect);

		world.RegisterSystem<FreeCamSystem>();

		return true;
	}

	bool Update(f32 deltaTime)
	{
		World& world = engine->GetWorld();
		world.Each<TransformComponent, MeshComponent>(
			[&](Entity entity, TransformComponent& transform, MeshComponent& meshComp)
			{
				transform.Rotate({ 0.f, 30.f * deltaTime, 0.f });
			});

		return true;
	}

	void OnResize(f32 deltaTime)
	{
	}
};

bool HookEngineFromApp(UserApplicationBase** outDesc)
{
	(*outDesc) = new TempGame();

	(*outDesc)->EngineInitDesc.Name        = "Test Bed";
	(*outDesc)->EngineInitDesc.WindowWidth  = 1920;
	(*outDesc)->EngineInitDesc.WindowHeight = 1080;

	return true;
}
