#include "Common/CommonTypes.h"
#include <Math/Math.h>

/*
These are mirrors of the GPU side structs that should be used to upload to the GPU
*/
struct LightInfo
{
	Vec3 position;
	float range;
	Vec3 color;
	float intensity;
	Vec3 direction;
	int32 type; // 0 = point, 1 = directional, 2 = spotlight
	float innerConeAngle;
	float outerConeAngle;
	Vec2 padding;
};

struct SkyParameters
{
	Vec3 sunDirection;
	float sunDiscSize;      // cosine of the sun disc half-angle (e.g. 0.9995)
	Vec3 skyColorZenith;
	float sunIntensity;
	Vec3 skyColorHorizon;
	float horizonSharpness; // controls how quickly horizon blends into zenith
	Vec3 groundColor;
	float groundFade;       // how quickly ground color blends below horizon
	Vec3 sunColor;
	float brightness; // 0 = sky disabled (black), >0 = scales entire sky output
};

struct LightPassConstants
{
	Mat4 invViewProj;
	Vec3 cameraPosition;
	int32 lightCount;
	SkyParameters sky;
	Mat4 lightViewProj;
	float shadowBias;
	Vec3 shadowPadding;
};