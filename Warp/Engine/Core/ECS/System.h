#pragma once

#include <Common/CommonTypes.h>

class World;

class WARP_API System
{
public:
	virtual ~System() = default;

	// Called once when the system is registered with the World.
	virtual void Init(World& world) {}

	// Called every frame in registration order.
	virtual void Update(World& world, f32 deltaTime) = 0;

	// Called when the system is unregistered or the World is destroyed.
	virtual void Shutdown() {}
};

template <typename T>
concept IsSystem = std::is_base_of_v<System, T>;
