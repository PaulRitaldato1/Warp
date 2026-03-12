#pragma once

#include <Common/CommonTypes.h>
#include <cstring>

enum RenderFlags : u32
{
	RenderFlags_None       = 0,
	RenderFlags_Visible    = 1 << 0,
	RenderFlags_CastShadow = 1 << 1,
	RenderFlags_ReceiveFog = 1 << 2,

	RenderFlags_Default = RenderFlags_Visible | RenderFlags_CastShadow | RenderFlags_ReceiveFog,
};

struct WARP_API MeshComponent
{
	char path[256]      = {}; // Asset path, null-terminated, fixed-size
	u32  renderFlags    = RenderFlags_Default;
	u32  meshHandle     = ~0u; // Cached resource handle — set once ready, avoids per-frame string lookup

	void SetPath(const char* assetPath)
	{
		const size_t len = std::min(std::strlen(assetPath), sizeof(path) - 1);
		std::memcpy(path, assetPath, len);
		path[len] = '\0';
	}

	void SetRenderFlag(RenderFlags flag)       { renderFlags |=  flag; }
	void ClearRenderFlag(RenderFlags flag)     { renderFlags &= ~flag; }
	bool HasRenderFlag(RenderFlags flag) const { return (renderFlags & flag) != 0; }

	bool IsHandleValid() const { return meshHandle != ~0u; }
};
