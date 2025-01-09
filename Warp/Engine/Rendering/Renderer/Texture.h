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
	RGBA8,
	RGB8,
	R8,
	R32F,
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

class ITexture
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

	virtual ~ITexture() = default;

private:
};