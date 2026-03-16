#pragma once

#include <Common/CommonTypes.h>
#include <Core/ECS/System.h>

class WARP_API OrbitSystem : public System
{
public:
	void Update(World& world, f32 deltaTime) override;

private:
	f32 m_elapsedTime = 0.f;
};
