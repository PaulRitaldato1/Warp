#pragma once

#include <Rendering/Mesh/Mesh.h>
#include <Threading/ThreadPool.h>

#include <future>

class MeshLoader
{
public:
	// Synchronous load — safe to call from any thread simultaneously.
	// All mesh primitives are flattened into a single vertex + index buffer.
	// Returns nullptr on failure.
	static URef<Mesh> Load(const String& path);

	// Submit a single load to a thread pool.
	// Returns a future the caller can wait on or poll.
	static std::future<URef<Mesh>> LoadAsync(const String& path, ThreadPool& pool);

	// Submit N loads to a thread pool in one call.
	// Returns one future per path, in the same order as the input.
	static Vector<std::future<URef<Mesh>>> LoadBatch(const Vector<String>& paths, ThreadPool& pool);
};
