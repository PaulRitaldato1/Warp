#pragma once

#include <Common/LoadError.h>
#include <Rendering/Mesh/Mesh.h>
#include <Threading/ThreadPool.h>

#include <future>

using MeshLoadResult = Expected<URef<Mesh>, LoadError>;

class MeshLoader
{
public:
	// Synchronous load — safe to call from any thread simultaneously.
	// All mesh primitives are flattened into a single vertex + index buffer.
	static MeshLoadResult Load(const String& path);

	// Submit a single load to a thread pool.
	// Returns a future the caller can wait on or poll.
	static std::future<MeshLoadResult> LoadAsync(const String& path, ThreadPool& pool);

	// Submit N loads to a thread pool in one call.
	// Returns one future per path, in the same order as the input.
	static Vector<std::future<MeshLoadResult>> LoadBatch(const Vector<String>& paths, ThreadPool& pool);
};
