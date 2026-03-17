#include "Core/ECS/Components/LightComponent.h"
#include <EntryPoint/EntryPoint.h>
#include <Common/CommonTypes.h>
#include <Input/Input.h>
#include <Core/ECS/Components/TransformComponent.h>
#include <Core/ECS/Components/MeshComponent.h>
#include <Core/ECS/Components/CameraComponent.h>
#include <Core/ECS/Components/OrbitComponent.h>
#include <Core/ECS/Systems/FreeCamSystem.h>
#include <Core/ECS/Systems/OrbitSystem.h>
#include <Rendering/Resource/ResourceManager.h>

struct TempGame : public UserApplicationBase
{
	bool Initialize()
	{
		World& world = engine->GetWorld();

		ResourceManager* resourceManager = engine->GetResourceManager();

		// Directional light
		// Entity directionalLightEntity	   = world.CreateEntity<TransformComponent, LightComponent>();
		// LightComponent& directionalLight   = world.GetComponent<LightComponent>(directionalLightEntity);
		// directionalLight.type			   = LightType::Directional;
		// directionalLight.color			   = { 1.f, 0.95f, 0.9f };
		// directionalLight.intensity		   = 0.1f;
		// TransformComponent& lightTransform = world.GetComponent<TransformComponent>(directionalLightEntity);
		// lightTransform.Rotate({ 45.f, -30.f, 0.f });

		// Floor plane
		u32 planeHandle										= resourceManager->CreatePlane(20.f, 20.f);
		Entity floor										= world.CreateEntity<TransformComponent, MeshComponent>();
		world.GetComponent<MeshComponent>(floor).meshHandle = planeHandle;
		world.GetComponent<TransformComponent>(floor).Move({ 0.f, -1.f, 0.f });

		// Objects on the floor
		Entity helmet = world.CreateEntity<TransformComponent, MeshComponent>();
		world.GetComponent<MeshComponent>(helmet).SetPath("Resources/DamagedHelmet/DamagedHelmet.gltf");
		world.GetComponent<TransformComponent>(helmet).Move({ 0.f, 0.f, 0.f });

		Entity avocado = world.CreateEntity<TransformComponent, MeshComponent>();
		world.GetComponent<MeshComponent>(avocado).SetPath("Resources/Avocado/Avocado.gltf");
		TransformComponent& avocadoTransform = world.GetComponent<TransformComponent>(avocado);
		avocadoTransform.Move({ 5.0f, -0.5f, 0.f });
		avocadoTransform.Scale(20.0f);

		// Create camera entity with FreeCam controls.
		const f32 aspect = static_cast<f32>(EngineInitDesc.WindowWidth) / static_cast<f32>(EngineInitDesc.WindowHeight);

		Entity camera = world.CreateEntity<TransformComponent, CameraComponent>();
		world.GetComponent<TransformComponent>(camera).Move({ 0.f, 0.f, -5.f });
		world.GetComponent<CameraComponent>(camera).SetAspect(aspect);

		// Orbiting point lights — red, green, purple — 120° apart
		constexpr f32 phaseStep = 2.0943951f; // 2*pi/3

		Entity redLight = world.CreateEntity<TransformComponent, LightComponent, OrbitComponent>();
		world.GetComponent<LightComponent>(redLight).color	   = { 1.f, 0.f, 0.f };
		world.GetComponent<LightComponent>(redLight).intensity = 0.3f;
		world.GetComponent<LightComponent>(redLight).range	   = 10.f;
		world.GetComponent<OrbitComponent>(redLight).radius	   = 5.f;
		world.GetComponent<OrbitComponent>(redLight).height	   = 1.f;

		Entity greenLight = world.CreateEntity<TransformComponent, LightComponent, OrbitComponent>();
		world.GetComponent<LightComponent>(greenLight).color	   = { 0.f, 1.f, 0.f };
		world.GetComponent<LightComponent>(greenLight).intensity   = 0.3f;
		world.GetComponent<LightComponent>(greenLight).range	   = 10.f;
		world.GetComponent<OrbitComponent>(greenLight).radius	   = 5.f;
		world.GetComponent<OrbitComponent>(greenLight).height	   = 1.f;
		world.GetComponent<OrbitComponent>(greenLight).phaseOffset = phaseStep;

		Entity purpleLight = world.CreateEntity<TransformComponent, LightComponent, OrbitComponent>();
		world.GetComponent<LightComponent>(purpleLight).color		= { 0.8f, 0.f, 1.f };
		world.GetComponent<LightComponent>(purpleLight).intensity	= 0.3f;
		world.GetComponent<LightComponent>(purpleLight).range		= 10.f;
		world.GetComponent<OrbitComponent>(purpleLight).radius		= 5.f;
		world.GetComponent<OrbitComponent>(purpleLight).height		= 1.f;
		world.GetComponent<OrbitComponent>(purpleLight).phaseOffset = phaseStep * 2.f;

		world.RegisterSystem<FreeCamSystem>();
		world.RegisterSystem<OrbitSystem>();

		return true;
	}

	bool Update(f32 deltaTime)
	{
		// World& world = engine->GetWorld();
		// world.Each<TransformComponent, MeshComponent>(
		// 	[&](Entity entity, TransformComponent& transform, MeshComponent& meshComp)
		// 	{ transform.Rotate({ 0.f, 30.f * deltaTime, 0.f }); });

		return true;
	}

	void OnResize(f32 deltaTime)
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
