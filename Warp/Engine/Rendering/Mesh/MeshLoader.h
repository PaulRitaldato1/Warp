#pragma once

#include <Common/LoadError.h>
#include <Rendering/Mesh/Mesh.h>

using MeshLoadResult = Expected<URef<Mesh>, LoadError>;

class MeshLoader
{
public:
	// Synchronous load — safe to call from any thread simultaneously.
	// All mesh primitives are flattened into a single vertex + index buffer.
	static MeshLoadResult Load(const String& path);
};
