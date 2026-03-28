#pragma once

#include <Math/Math.h>

struct WARP_API SkyLightComponent
{
	// Sky gradient
	Vec3 skyColorZenith   = { 0.1f, 0.2f, 0.45f };
	Vec3 skyColorHorizon  = { 0.55f, 0.65f, 0.8f };
	f32  horizonSharpness = 0.35f;

	// Ground (below horizon)
	Vec3 groundColor = { 0.25f, 0.22f, 0.2f };
	f32  groundFade  = 3.0f;

	// Sun disc (direction comes from the entity's TransformComponent)
	Vec3 sunColor     = { 1.0f, 0.95f, 0.8f };
	f32  sunIntensity = 2.0f;
	f32  sunDiscSize  = 0.9995f; // cosine of the sun disc half-angle

	// Built-in directional light — color comes from sunColor, direction from transform
	f32 lightIntensity = 1.0f;
};
