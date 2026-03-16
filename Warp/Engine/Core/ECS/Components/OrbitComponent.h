#pragma once

#include <Common/CommonTypes.h>

struct WARP_API OrbitComponent
{
	f32 radius      = 5.f;
	f32 speed       = 1.f;    // radians per second
	f32 height      = 0.f;
	f32 phaseOffset = 0.f;    // starting angle offset in radians
};
