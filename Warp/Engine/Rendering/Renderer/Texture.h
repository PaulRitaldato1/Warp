#pragma once

#include <Common/CommonTypes.h>

enum class TextureType
{
	Texture2D,
	Texture3D,
	CubeMap
};

enum class TextureFormat
{
	Unknown,

	// LDR uncompressed
	RGBA8,          // 4x u8, linear
	RGBA8_SRGB,     // 4x u8, sRGB (albedo / diffuse textures)
	RGB8,           // 3x u8, linear
	RG8,            // 2x u8 (packed normal maps)
	R8,             // 1x u8 (roughness, AO, masks)

	// HDR uncompressed
	RGBA16F,        // 4x f16 (GPU-resident HDR)
	RGBA32F,        // 4x f32 (CPU-loaded HDR from stb_image)
	R32F,           // 1x f32 (shadow maps, depth)

	// BC compressed (added when DDS support lands)
	BC1,            // opaque or 1-bit alpha
	BC3,            // full alpha
	BC4,            // single channel
	BC5,            // two channel (normal maps)
	BC7,            // high-quality RGBA

	// Depth
	Depth24Stencil8
};

enum class TextureUsage
{
	Sampled,	  // Used for sampling in shaders
	RenderTarget, // Used as Render Target
	DepthStencil, // Used as Depth Stencil Target
	Storage,	  // Used for storage in shaders
	TransferSrc,  // Source for data transfer
	TransferDst	  // Dest for data transfer
};

struct TextureDesc
{
	TextureType Type;
	u32 Width;
	u32 Height;
	u32 Depth		= 1; // for 3D textures
	u32 MipLevels	= 1;
	u32 ArrayLayers = 1;
	TextureFormat Format;
	bool bGenerateMipMaps = false;
	TextureUsage Usage;
};

class Texture
{
public:
	virtual void Init(const TextureDesc& Desc) = 0;

	// Generate mipmaps (if supported)
	virtual void GenerateMipmaps() = 0;

	// Get native handle (platform-specific)
	virtual void* GetNativeHandle() const = 0;

	// Bind the texture to a specific slot (if applicable)
	virtual void Bind(uint32_t slot) const = 0;

	// Get texture width
	virtual u32 GetWidth() const = 0;

	// Get texture height
	virtual u32 GetHeight() const = 0;

	// Get texture depth (for 3D textures)
	virtual u32 GetDepth() const = 0;

	// Get number of mip levels
	virtual u32 GetMipLevels() const = 0;

	virtual ~Texture() = default;

private:
};