#pragma once

#include <Common/CommonTypes.h>
#include <Math/Math.h>
#include <Rendering/Lighting/LightData.h>
#include <Rendering/Renderer/DrawList.h>

class World;

struct CameraView
{
	Mat4 viewProj		   = {};
	Mat4 invViewProj	   = {};
	Vec3 position		   = {};
	Array<Vec4, 6> frustum = {};
};

// The first SkyLightComponent drives the sky, the sun and the one directional shadow map.
struct SkyView
{
	SkyParameters parameters = {};

	bool bHasSun  = false;
	LightInfo sun = {};

	// The sun's shadow view, only set with bHasSun. Built here rather than in the
	// shadow pass because the GPU scene culls shadow casters against it first.
	Mat4 sunViewProj		  = {};
	Array<Vec4, 6> sunFrustum = {};
};

// What the frame sees, gathered from the world once before any pass is added.
// Passes read what they need from it, so none of them touch the ECS.
struct FrameView
{
	u32 width  = 0;
	u32 height = 0;

	CameraView camera;
	SkyView sky;
	LightList lights;
};

// Without an active camera the camera fields stay zero and an error is logged.
FrameView BuildFrameView(World& world, u32 width, u32 height);
