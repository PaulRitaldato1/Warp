#pragma once

#include <Common/CommonTypes.h>
#include <Rendering/Renderer/Texture.h>

// ---------------------------------------------------------------------------
// TextureColorSpace — tells the loader / GPU what transfer function to use.
// ---------------------------------------------------------------------------
enum class TextureColorSpace : u8
{
	sRGB,   // base color, emissive — stored as RGBA8_SRGB
	Linear  // normal, roughness, AO — stored as RGBA8
};

// ---------------------------------------------------------------------------
// MipData — one mip level's worth of raw bytes and layout info.
// For BC formats the data is already compressed — upload directly to the GPU.
// ---------------------------------------------------------------------------
struct MipData
{
	const u8* data       = nullptr; // non-owning view into TextureData::data
	u64       rowPitch   = 0;       // bytes per row (aligned for BC: ceil(w/4)*blockBytes)
	u64       slicePitch = 0;       // rowPitch * rows (rows = ceil(h/4) for BC)
	u32       width      = 0;
	u32       height     = 0;
};

// ---------------------------------------------------------------------------
// TextureData — CPU-side DDS contents ready for GPU upload.
// All raw bytes live in `data`; MipData entries are non-owning views into it.
// ---------------------------------------------------------------------------
struct TextureData
{
	String              name;
	u32                 width      = 0;
	u32                 height     = 0;
	u32                 depth      = 1;     // > 1 for 3D textures
	u32                 arraySize  = 1;     // > 1 for texture arrays / cubemaps (6)
	u32                 mipLevels  = 1;
	TextureFormat       format     = TextureFormat::Unknown;
	TextureType         type       = TextureType::Texture2D;
	bool                isCubemap  = false;

	// Raw pixel / compressed data for the entire texture (all mips, all slices).
	Vector<u8>          data;

	// Per-mip layout — indexed [arraySlice * mipLevels + mipIndex].
	// For a simple 2D texture: mips[0] is the top-level mip, mips[1] the next, etc.
	Vector<MipData>     mips;

	// Returns bytes per pixel for uncompressed formats, 0 for BC formats
	// (BC textures use rowPitch/slicePitch from MipData instead).
	u32 BytesPerPixel() const;
};
