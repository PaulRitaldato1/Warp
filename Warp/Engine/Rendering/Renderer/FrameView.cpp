#include <Core/ECS/Components/CameraComponent.h>
#include <Core/ECS/Components/LightComponent.h>
#include <Core/ECS/Components/SkyLightComponent.h>
#include <Core/ECS/Components/TransformComponent.h>
#include <Core/ECS/World.h>
#include <Debugging/Logging.h>
#include <Math/Frustum.h>
#include <Rendering/Renderer/FrameView.h>

#include <cmath>

static CameraView GatherCamera(World& world)
{
	CameraView camera;
	bool bHasCamera = false;

	world.Each<const TransformComponent, const CameraComponent>(
		[&](Entity entity, const TransformComponent& transform, const CameraComponent& cameraComp)
		{
			if (cameraComp.isActive)
			{
				camera.viewProj = cameraComp.viewProj;
				camera.position = transform.position;
				bHasCamera		= true;
			}
		});

	if (!bHasCamera)
	{
		LOG_ERROR("BuildFrameView: No valid Camera in the world");
	}

	using namespace DirectX;
	XMStoreFloat4x4(&camera.invViewProj, XMMatrixInverse(nullptr, XMLoadFloat4x4(&camera.viewProj)));
	camera.frustum = ExtractFrustumPlanes(camera.viewProj);

	return camera;
}

// An orthographic box around the origin, looking along the sun.
static Mat4 BuildSunViewProj(const Vec3& sunDirection)
{
	using namespace DirectX;

	XMVECTOR lightDir = XMVector3Normalize(XMLoadFloat3(&sunDirection));
	XMVECTOR lightPos = XMVectorScale(XMVectorNegate(lightDir), 50.f);
	XMVECTOR up		  = XMVectorSet(0.f, 1.f, 0.f, 0.f);

	// Avoid degenerate up vector when light points nearly straight up or down.
	if (fabsf(XMVectorGetY(lightDir)) > 0.99f)
	{
		up = XMVectorSet(1.f, 0.f, 0.f, 0.f);
	}

	XMMATRIX lightView = XMMatrixLookToLH(lightPos, lightDir, up);

	float orthoHalfSize = 30.f;
	float nearPlane		= 0.1f;
	float farPlane		= 100.f;
	XMMATRIX lightProj	= XMMatrixOrthographicLH(orthoHalfSize * 2.f, orthoHalfSize * 2.f, nearPlane, farPlane);

	Mat4 sunViewProj;
	XMStoreFloat4x4(&sunViewProj, XMMatrixMultiply(lightView, lightProj));
	return sunViewProj;
}

static SkyView GatherSky(World& world)
{
	SkyView sky;

	world.Each<const TransformComponent, const SkyLightComponent>(
		[&](Entity entity, const TransformComponent& transform, const SkyLightComponent& skyComp)
		{
			if (sky.bHasSun)
			{
				return; // use the first one
			}

			SkyParameters& parameters	= sky.parameters;
			parameters.skyColorZenith	= skyComp.skyColorZenith;
			parameters.skyColorHorizon	= skyComp.skyColorHorizon;
			parameters.horizonSharpness = skyComp.horizonSharpness;
			parameters.groundColor		= skyComp.groundColor;
			parameters.groundFade		= skyComp.groundFade;
			parameters.sunColor			= skyComp.sunColor;
			parameters.sunIntensity		= skyComp.sunIntensity;
			parameters.sunDiscSize		= skyComp.sunDiscSize;
			parameters.brightness		= skyComp.lightIntensity;

			const Vec3 sunDirection = transform.Forward();
			{
				using namespace DirectX;
				XMVECTOR sunDir = XMVector3Normalize(XMLoadFloat3(&sunDirection));
				XMStoreFloat3(&parameters.sunDirection, XMVectorNegate(sunDir));
			}

			LightInfo& sun	   = sky.sun;
			sun.position	   = {};
			sun.range		   = 0.f;
			sun.color		   = skyComp.sunColor;
			sun.intensity	   = skyComp.lightIntensity;
			sun.direction	   = sunDirection;
			sun.type		   = 1; // Directional
			sun.innerConeAngle = 0.f;
			sun.outerConeAngle = 0.f;
			sun.padding		   = {};

			sky.bHasSun = true;
		});

	if (sky.bHasSun)
	{
		sky.sunViewProj = BuildSunViewProj(sky.sun.direction);

		// Gribb-Hartmann needs no special case for an orthographic matrix, which is
		// why the camera and the light share one extraction path.
		sky.sunFrustum = ExtractFrustumPlanes(sky.sunViewProj);
	}

	return sky;
}

static LightList GatherLights(World& world)
{
	LightList lights;

	world.Each<const TransformComponent, const LightComponent>(
		[&](Entity entity, const TransformComponent& transform, const LightComponent& lightComp)
		{
			LightItem item;
			item.position		= transform.position;
			item.direction		= transform.Forward();
			item.color			= lightComp.color;
			item.intensity		= lightComp.intensity;
			item.range			= lightComp.range;
			item.innerConeAngle = lightComp.innerConeAngle;
			item.outerConeAngle = lightComp.outerConeAngle;
			item.type			= lightComp.type;
			item.castShadows	= lightComp.castShadows;

			const u32 itemIndex = static_cast<u32>(lights.items.size());
			lights.items.push_back(item);

			if (lightComp.castShadows)
			{
				lights.shadowCasters.push_back(itemIndex);
			}
		});

	return lights;
}

FrameView BuildFrameView(World& world, u32 width, u32 height)
{
	FrameView view;
	view.width	= width;
	view.height = height;
	view.camera = GatherCamera(world);
	view.sky	= GatherSky(world);
	view.lights = GatherLights(world);
	return view;
}
