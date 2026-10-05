#pragma once

#include <Common/CommonTypes.h>

#ifndef WARP_RELEASE

#include <Debugging/renderdoc_app.h>

// Must be constructed before the graphics device, since RenderDoc hooks the API
// when the device is created.
class RenderDocCapture
{
public:
	RenderDocCapture();
	~RenderDocCapture() = default;

	bool IsAvailable() const { return m_api != nullptr; }

private:
	RENDERDOC_API_1_7_0* m_api = nullptr;
};

#else // WARP_RELEASE — stub, all calls compile away to nothing

class RenderDocCapture
{
public:
	bool IsAvailable() const { return false; }
};

#endif // WARP_RELEASE
