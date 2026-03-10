#pragma once

#include <Common/CommonTypes.h>

class Camera
{
public:
	virtual ~Camera() = default;

	virtual void Update(f32 deltaTime)           = 0;
	virtual Mat4 GetViewProjectionMatrix() const = 0;
};
