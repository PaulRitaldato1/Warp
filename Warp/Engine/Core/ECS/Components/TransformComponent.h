#pragma once

#include <Math/Math.h>

struct WARP_API TransformComponent
{
	Vec3 position = { 0.f, 0.f, 0.f };
	Quat rotation = { 0.f, 0.f, 0.f, 1.f }; // identity quaternion (x, y, z, w)
	Vec3 scale	  = { 1.f, 1.f, 1.f };

	// Translate by a delta in world space.
	void Move(Vec3 delta)
	{
		position.x += delta.x;
		position.y += delta.y;
		position.z += delta.z;
	}

	// Rotate by Euler angles (degrees) around X, Y, Z axes respectively.
	void Rotate(Vec3 eulerDegrees)
	{
		using namespace DirectX;
		SimdVec current = XMLoadFloat4(&rotation);
		SimdVec delta   = XMQuaternionRotationRollPitchYaw(
			XMConvertToRadians(eulerDegrees.x),
			XMConvertToRadians(eulerDegrees.y),
			XMConvertToRadians(eulerDegrees.z));
		XMStoreFloat4(&rotation, XMQuaternionMultiply(current, delta));
	}

	// Multiply scale uniformly on all axes.
	void Scale(f32 factor)
	{
		scale.x *= factor;
		scale.y *= factor;
		scale.z *= factor;
	}

	// Multiply scale per-axis.
	void Scale(Vec3 factors)
	{
		scale.x *= factors.x;
		scale.y *= factors.y;
		scale.z *= factors.z;
	}
};
