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

struct LightPassConstants
{
	Mat4 invViewProj;
	Vec3 cameraPosition;
	int32 lightCount;
};