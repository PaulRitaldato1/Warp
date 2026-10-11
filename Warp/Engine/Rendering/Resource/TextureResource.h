#pragma once

#include <Common/CommonTypes.h>
#include <Rendering/Resource/ResourceState.h>
#include <Rendering/Renderer/DescriptorHandle.h>
#include <Rendering/Texture/TextureData.h>
#include <Rendering/Renderer/Texture.h>

struct TextureResource
{
	u32        handle = ~0u;
	AssetState state  = AssetState::Loading;

	// What it was loaded as. The cache is keyed by path, so a second request for
	// the same file as the other color space gets this one and warns.
	TextureColorSpace colorSpace = TextureColorSpace::sRGB;

	// CPU-side data — retained for mip layout info.
	URef<TextureData> textureData;

	// GPU-side texture resource.
	URef<Texture> gpuTexture;

	// SRV handle for shader binding.
	DescriptorHandle descriptor;
};
