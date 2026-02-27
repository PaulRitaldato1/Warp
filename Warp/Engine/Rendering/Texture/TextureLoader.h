#pragma once

#include <Rendering/Texture/TextureData.h>
#include <Threading/ThreadPool.h>

#include <future>

class TextureLoader
{
public:
	// Synchronous load of a DDS file from disk.
	// Parses all mip levels, array slices, and cubemap faces.
	// Returns nullptr on failure.
	static URef<TextureData> Load(const String& path);

	// Submit a single load to a thread pool.
	static std::future<URef<TextureData>> LoadAsync(const String& path, ThreadPool& pool);

	// Submit N loads to a thread pool in one call.
	// Returns one future per path, in the same order as the input.
	static Vector<std::future<URef<TextureData>>> LoadBatch(const Vector<String>& paths, ThreadPool& pool);
};
