#pragma once

#include <Common/CommonTypes.h>
#include <Rendering/Resource/ResourceState.h>
#include <Rendering/Renderer/DescriptorHandle.h>
#include <Rendering/Texture/TextureData.h>
#include <Rendering/Renderer/Texture.h>

struct TextureResource
{
	AssetState state = AssetState::Loading;

	// CPU-side data — retained for mip layout info.
	URef<TextureData> textureData;

	// GPU-side texture resource.
	URef<Texture> gpuTexture;

	// SRV handle for shader binding.
	DescriptorHandle descriptor;

	// Frame counter for tracking upload completion.
	u32 uploadFrameCounter = 0;
};
