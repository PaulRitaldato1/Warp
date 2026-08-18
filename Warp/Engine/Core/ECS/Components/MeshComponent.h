#pragma once

#include <Common/CommonTypes.h>
#include <Common/PathRegistry.h>

enum RenderFlags : u32
{
	RenderFlags_None	   = 0,
	RenderFlags_Visible	   = 1 << 0,
	RenderFlags_CastShadow = 1 << 1,
	RenderFlags_ReceiveFog = 1 << 2,
	RenderFlags_Unlit	   = 1 << 3,
	RenderFlags_Default	   = RenderFlags_Visible | RenderFlags_CastShadow | RenderFlags_ReceiveFog,
};

// The asset path is interned rather than stored inline. The gather loop walks
// this column every frame and reads only the flags and the handle
struct WARP_API MeshComponent
{
	u32 renderFlags = RenderFlags_Default;
	u32 meshHandle	= ~0u; // Cached resource handle, set once ready
	u32 pathId		= PathRegistry::k_invalidId;

	void SetPath(const char* assetPath)
	{
		pathId = GetPathRegistry().Intern(assetPath);
	}

	void ClearPath()
	{
		pathId = PathRegistry::k_invalidId;
	}

	// Empty string when no path is set.
	const String& GetPath() const
	{
		return GetPathRegistry().Resolve(pathId);
	}

	void SetRenderFlag(RenderFlags flag)
	{
		renderFlags |= flag;
	}
	void ClearRenderFlag(RenderFlags flag)
	{
		renderFlags &= ~flag;
	}
	bool HasRenderFlag(RenderFlags flag) const
	{
		return (renderFlags & flag) != 0;
	}

	bool IsHandleValid() const
	{
		return meshHandle != ~0u;
	}

	bool HasPath() const
	{
		return pathId != PathRegistry::k_invalidId;
	}

	bool IsValid() const
	{
		return IsHandleValid() || HasPath();
	}
};
