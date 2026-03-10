#pragma once

#include <Common/CommonTypes.h>
#include <cstring>

struct WARP_API MeshComponent
{
	char path[256] = {}; // Asset path, null-terminated, fixed-size

	void SetPath(const char* assetPath)
	{
		const size_t len = std::min(std::strlen(assetPath), sizeof(path) - 1);
		std::memcpy(path, assetPath, len);
		path[len] = '\0';
	}
};
