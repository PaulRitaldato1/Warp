#pragma once

#include <Math/Math.h>

struct WARP_API CameraComponent
{
	Mat4 viewProj  = {};
	f32  fovY      = 60.f;
	f32  aspect    = 16.f / 9.f;
	f32  nearPlane = 0.1f;
	f32  farPlane  = 1000.f;
	bool isActive  = true;

	void SetFovY(f32 degrees)
	{
		fovY = degrees;
	}

	void SetAspect(f32 aspectRatio)
	{
		aspect = aspectRatio;
	}

	void SetNearPlane(f32 nearZ)
	{
		nearPlane = nearZ;
	}

	void SetFarPlane(f32 farZ)
	{
		farPlane = farZ;
	}

	void SetActive(bool active)
	{
		isActive = active;
	}
};
