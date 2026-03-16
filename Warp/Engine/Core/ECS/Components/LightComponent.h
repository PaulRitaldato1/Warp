#pragma once

#include <Math/Math.h>

enum class LightType : u8
{
	Point,
	Directional,
	Spot,
};

struct WARP_API LightComponent
{
	LightType type      = LightType::Point;
	Vec3      color     = { 1.f, 1.f, 1.f };
	f32       intensity = 1.f;

	// Point + Spot
	f32 range = 10.f;

	// Spot only
	f32 innerConeAngle = 15.f; // degrees
	f32 outerConeAngle = 30.f; // degrees

	bool castShadows = false;
};
