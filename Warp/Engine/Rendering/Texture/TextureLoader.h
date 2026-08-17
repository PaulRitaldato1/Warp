#pragma once

#include <Common/LoadError.h>
#include <Rendering/Texture/TextureData.h>
#include <Threading/ThreadPool.h>

#include <future>

using TextureLoadResult = Expected<URef<TextureData>, LoadError>;

class TextureLoader
{
public:
	// Synchronous load of an image file from disk. DDS parses all mip levels,
	// array slices, and cubemap faces; other formats generate mips on the CPU.
	static TextureLoadResult Load(const String& path,
	                              TextureColorSpace colorSpace = TextureColorSpace::sRGB);

	// Submit a single load to a thread pool.
	static std::future<TextureLoadResult> LoadAsync(const String& path, ThreadPool& pool,
	                                                TextureColorSpace colorSpace = TextureColorSpace::sRGB);

	// Submit N loads to a thread pool in one call.
	// Returns one future per path, in the same order as the input.
	static Vector<std::future<TextureLoadResult>> LoadBatch(const Vector<String>& paths, ThreadPool& pool,
	                                                        TextureColorSpace colorSpace = TextureColorSpace::sRGB);
};
