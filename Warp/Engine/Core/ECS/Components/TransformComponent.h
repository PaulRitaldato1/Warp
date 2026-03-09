#pragma once

#include <Math/Math.h>

struct WARP_API TransformComponent
{
	Vec3 position = { 0.f, 0.f, 0.f };
	Quat rotation = { 0.f, 0.f, 0.f, 1.f }; // identity quaternion (x, y, z, w)
	Vec3 scale	  = { 1.f, 1.f, 1.f };
};
