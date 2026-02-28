#pragma once

#include <Common/CommonTypes.h>

enum class TextureType
{
	Texture2D,
	Texture3D,
	CubeMap,
};

// Unified format enum used for both texture resources and vertex input element formats.
// Values map 1:1 to DXGI_FORMAT on D3D12.
enum class TextureFormat
{
	Unknown,

	// LDR uncompressed
	RGBA8,           // DXGI_FORMAT_R8G8B8A8_UNORM
	RGBA8_SRGB,      // DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
	BGRA8,           // DXGI_FORMAT_B8G8R8A8_UNORM  — Windows swap-chain default
	RG8,             // DXGI_FORMAT_R8G8_UNORM
	R8,              // DXGI_FORMAT_R8_UNORM

	// HDR uncompressed
	RGBA16F,         // DXGI_FORMAT_R16G16B16A16_FLOAT
	RGBA32F,         // DXGI_FORMAT_R32G32B32A32_FLOAT  — Vec4 (tangent / color)
	RGB32F,          // DXGI_FORMAT_R32G32B32_FLOAT     — Vec3 (position / normal)
	RG32F,           // DXGI_FORMAT_R32G32_FLOAT        — Vec2 (UV)
	R32F,            // DXGI_FORMAT_R32_FLOAT

	// BC compressed
	BC1,             // DXGI_FORMAT_BC1_UNORM  — opaque or 1-bit alpha
	BC3,             // DXGI_FORMAT_BC3_UNORM  — full alpha
	BC4,             // DXGI_FORMAT_BC4_UNORM  — single channel
	BC5,             // DXGI_FORMAT_BC5_UNORM  — two-channel (normal maps)
	BC7,             // DXGI_FORMAT_BC7_UNORM  — high-quality RGBA

	// Depth
	Depth24Stencil8, // DXGI_FORMAT_D24_UNORM_S8_UINT
	Depth32F,        // DXGI_FORMAT_D32_FLOAT
};

enum class TextureUsage
{
	Sampled,      // read in shaders
	RenderTarget, // colour output
	DepthStencil, // depth/stencil output
	Storage,      // read/write in compute shaders
};

struct TextureDesc
{
	TextureType   type        = TextureType::Texture2D;
	u32           width       = 1;
	u32           height      = 1;
	u32           depth       = 1;  // for Texture3D
	u32           mipLevels   = 1;
	u32           arrayLayers = 1;
	TextureFormat format      = TextureFormat::RGBA8;
	TextureUsage  usage       = TextureUsage::Sampled;
};

class Texture
{
public:
	virtual ~Texture() = default;

	virtual void  Init(const TextureDesc& desc)  = 0;
	virtual void  Cleanup()                      = 0;

	// Returns the underlying API resource pointer (e.g. ID3D12Resource*).
	virtual void* GetNativeHandle() const  = 0;

	virtual u32           GetWidth()     const = 0;
	virtual u32           GetHeight()    const = 0;
	virtual u32           GetDepth()     const = 0;
	virtual u32           GetMipLevels() const = 0;
	virtual TextureFormat GetFormat()    const = 0;
};
