#include <Core/ECS/Systems/OrbitSystem.h>
#include <Core/ECS/World.h>
#include <Core/ECS/Components/TransformComponent.h>
#include <Core/ECS/Components/OrbitComponent.h>
#include <cmath>

void OrbitSystem::Update(World& world, f32 deltaTime)
{
	m_elapsedTime += deltaTime;

	world.Each<TransformComponent, OrbitComponent>(
		[&](Entity entity, TransformComponent& transform, OrbitComponent& orbit)
		{
			f32 angle = m_elapsedTime * orbit.speed + orbit.phaseOffset;
			transform.position.x = std::cos(angle) * orbit.radius;
			transform.position.y = orbit.height;
			transform.position.z = std::sin(angle) * orbit.radius;
		});
}
