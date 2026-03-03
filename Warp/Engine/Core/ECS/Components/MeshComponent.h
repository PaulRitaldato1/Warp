#pragma once

#include <Common/CommonTypes.h>
#include <cstring>

struct MeshComponent
{
	char path[256] = {}; // Asset path, null-terminated, fixed-size

	void SetPath(const char* assetPath)
	{
		std::strncpy(path, assetPath, sizeof(path) - 1);
		path[sizeof(path) - 1] = '\0';
	}
};
