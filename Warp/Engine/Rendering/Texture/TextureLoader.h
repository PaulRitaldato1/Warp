#pragma once

#include <Common/LoadError.h>
#include <Rendering/Texture/TextureData.h>

using TextureLoadResult = Expected<URef<TextureData>, LoadError>;

class TextureLoader
{
public:
	// Synchronous load of an image file from disk. DDS parses all mip levels,
	// array slices, and cubemap faces; other formats generate mips on the CPU.
	static TextureLoadResult Load(const String& path,
	                              TextureColorSpace colorSpace = TextureColorSpace::sRGB);
};
