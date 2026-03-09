#include <Rendering/Texture/TextureLoader.h>
#include <Threading/ThreadPool.h>
#include <Debugging/Logging.h>

#define TINYDDSLOADER_IMPLEMENTATION
#include <tinyddsloader.h>

#define STB_IMAGE_IMPLEMENTATION
#ifdef _WIN32
#define STBI_NO_SIMD  // __cpuid macro in cpuid.h conflicts with stb_image's SIMD detection
#endif
#include <stb_image.h>

#include <filesystem>
#include <cstring>

// ---------------------------------------------------------------------------
// Format mapping — tinyddsloader DXGI format → engine TextureFormat
// ---------------------------------------------------------------------------

static TextureFormat TranslateDXGIFormat(tinyddsloader::DDSFile::DXGIFormat fmt)
{
	using F = tinyddsloader::DDSFile::DXGIFormat;
	switch (fmt)
	{
		case F::R8G8B8A8_UNorm:      return TextureFormat::RGBA8;
		case F::R8G8B8A8_UNorm_SRGB: return TextureFormat::RGBA8_SRGB;
		case F::R8G8_UNorm:          return TextureFormat::RG8;
		case F::R8_UNorm:            return TextureFormat::R8;
		case F::R16G16B16A16_Float:  return TextureFormat::RGBA16F;
		case F::R32G32B32A32_Float:  return TextureFormat::RGBA32F;
		case F::R32_Float:           return TextureFormat::R32F;
		case F::BC1_UNorm:           return TextureFormat::BC1;
		case F::BC1_UNorm_SRGB:      return TextureFormat::BC1;
		case F::BC3_UNorm:           return TextureFormat::BC3;
		case F::BC3_UNorm_SRGB:      return TextureFormat::BC3;
		case F::BC4_UNorm:           return TextureFormat::BC4;
		case F::BC5_UNorm:           return TextureFormat::BC5;
		case F::BC7_UNorm:           return TextureFormat::BC7;
		case F::BC7_UNorm_SRGB:      return TextureFormat::BC7;
		default:
			return TextureFormat::Unknown;
	}
}

// ---------------------------------------------------------------------------
// BytesPerPixel — used for uncompressed uploads only.
// BC formats use the per-mip rowPitch / slicePitch from tinyddsloader.
// ---------------------------------------------------------------------------

u32 TextureData::BytesPerPixel() const
{
	switch (format)
	{
		case TextureFormat::RGBA8:
		case TextureFormat::RGBA8_SRGB: return 4;
		case TextureFormat::RG8:        return 2;
		case TextureFormat::R8:         return 1;
		case TextureFormat::RGBA16F:    return 8;
		case TextureFormat::RGBA32F:    return 16;
		case TextureFormat::R32F:       return 4;
		// BC formats: block-based, use MipData::rowPitch instead
		default:                        return 0;
	}
}

// ---------------------------------------------------------------------------
// File format detection
// ---------------------------------------------------------------------------

enum class ImageFileFormat { DDS, STB };

static ImageFileFormat DetectFormat(const String& path)
{
	String ext = std::filesystem::path(path).extension().string();
	std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
	if (ext == ".dds")
	{
		return ImageFileFormat::DDS;
	}
	return ImageFileFormat::STB;
}

// ---------------------------------------------------------------------------
// Mip generation helpers
// ---------------------------------------------------------------------------

static u32 CalculateMipCount(u32 width, u32 height)
{
	return static_cast<u32>(std::floor(std::log2(std::max(width, height)))) + 1;
}

// Box-filter downsample: averages 2x2 blocks of RGBA8 pixels.
static void GenerateNextMip(const u8* src, u32 srcWidth, u32 srcHeight,
                             u8* dst, u32 dstWidth, u32 dstHeight)
{
	for (u32 y = 0; y < dstHeight; ++y)
	{
		for (u32 x = 0; x < dstWidth; ++x)
		{
			u32 sx = x * 2;
			u32 sy = y * 2;
			u32 sx1 = std::min(sx + 1, srcWidth  - 1);
			u32 sy1 = std::min(sy + 1, srcHeight - 1);

			const u8* p00 = src + (sy  * srcWidth + sx)  * 4;
			const u8* p10 = src + (sy  * srcWidth + sx1) * 4;
			const u8* p01 = src + (sy1 * srcWidth + sx)  * 4;
			const u8* p11 = src + (sy1 * srcWidth + sx1) * 4;

			for (u32 c = 0; c < 4; ++c)
			{
				dst[(y * dstWidth + x) * 4 + c] = static_cast<u8>(
					(p00[c] + p10[c] + p01[c] + p11[c] + 2) / 4);
			}
		}
	}
}

// ---------------------------------------------------------------------------
// LoadDDS — tinyddsloader path (existing logic)
// ---------------------------------------------------------------------------

static URef<TextureData> LoadDDS(const String& path)
{
	namespace fs = std::filesystem;
	using namespace tinyddsloader;

	DDSFile dds;
	const auto result = dds.Load(path.c_str());

	if (result != tinyddsloader::Success)
	{
		LOG_ERROR("TextureLoader: failed to load '{}'", path);
		return nullptr;
	}

	const TextureFormat format = TranslateDXGIFormat(dds.GetFormat());
	if (format == TextureFormat::Unknown)
	{
		LOG_ERROR("TextureLoader: unsupported DXGI format in '{}'", path);
		return nullptr;
	}

	URef<TextureData> tex  = std::make_unique<TextureData>();
	tex->name       = fs::path(path).stem().string();
	tex->width      = dds.GetWidth();
	tex->height     = dds.GetHeight();
	tex->depth      = dds.GetDepth();
	tex->mipLevels  = dds.GetMipCount();
	tex->arraySize  = dds.GetArraySize();
	tex->format     = format;
	tex->isCubemap  = dds.IsCubemap();
	tex->type       = dds.IsCubemap()        ? TextureType::CubeMap
	                : (dds.GetDepth() > 1)   ? TextureType::Texture3D
	                                         : TextureType::Texture2D;

	// --- Copy all mip data into a single owned buffer ---
	// Calculate total size first to avoid reallocations.
	u64 totalBytes = 0;
	for (u32 slice = 0; slice < tex->arraySize; ++slice)
	{
		for (u32 mip = 0; mip < tex->mipLevels; ++mip)
		{
			const auto* imageData = dds.GetImageData(mip, slice);
			if (imageData)
			{
				totalBytes += imageData->m_memSlicePitch;
			}
		}
	}

	tex->data.resize(totalBytes);
	tex->mips.reserve(tex->arraySize * tex->mipLevels);

	u64 offset = 0;
	for (u32 slice = 0; slice < tex->arraySize; ++slice)
	{
		for (u32 mip = 0; mip < tex->mipLevels; ++mip)
		{
			const auto* src = dds.GetImageData(mip, slice);
			if (!src)
			{
				continue;
			}

			std::memcpy(tex->data.data() + offset, src->m_mem, src->m_memSlicePitch);

			MipData mipEntry;
			mipEntry.data       = tex->data.data() + offset;
			mipEntry.rowPitch   = src->m_memPitch;
			mipEntry.slicePitch = src->m_memSlicePitch;
			mipEntry.width      = std::max(1u, tex->width  >> mip);
			mipEntry.height     = std::max(1u, tex->height >> mip);
			tex->mips.push_back(mipEntry);

			offset += src->m_memSlicePitch;
		}
	}

	LOG_DEBUG("TextureLoader: loaded '{}' — {}x{}, {} mips{}",
	          tex->name, tex->width, tex->height, tex->mipLevels,
	          tex->isCubemap ? ", cubemap" : "");

	return tex;
}

// ---------------------------------------------------------------------------
// LoadSTB — PNG/JPG/BMP/TGA path with CPU mip generation
// ---------------------------------------------------------------------------

static URef<TextureData> LoadSTB(const String& path, TextureColorSpace colorSpace)
{
	int w, h, channels;
	u8* pixels = stbi_load(path.c_str(), &w, &h, &channels, 4); // force RGBA
	if (!pixels)
	{
		LOG_ERROR("TextureLoader: stb_image failed to load '{}'", path);
		return nullptr;
	}

	u32 width  = static_cast<u32>(w);
	u32 height = static_cast<u32>(h);
	u32 mipCount = CalculateMipCount(width, height);

	TextureFormat format = (colorSpace == TextureColorSpace::sRGB)
	                       ? TextureFormat::RGBA8_SRGB
	                       : TextureFormat::RGBA8;

	// Calculate total size for all mips.
	u64 totalBytes = 0;
	{
		u32 mw = width, mh = height;
		for (u32 m = 0; m < mipCount; ++m)
		{
			totalBytes += static_cast<u64>(mw) * mh * 4;
			mw = std::max(1u, mw / 2);
			mh = std::max(1u, mh / 2);
		}
	}

	URef<TextureData> tex = std::make_unique<TextureData>();
	tex->name      = std::filesystem::path(path).stem().string();
	tex->width     = width;
	tex->height    = height;
	tex->depth     = 1;
	tex->arraySize = 1;
	tex->mipLevels = mipCount;
	tex->format    = format;
	tex->type      = TextureType::Texture2D;
	tex->isCubemap = false;

	tex->data.resize(totalBytes);
	tex->mips.reserve(mipCount);

	// Mip 0: copy from stb_image.
	u64 mip0Size = static_cast<u64>(width) * height * 4;
	std::memcpy(tex->data.data(), pixels, mip0Size);
	stbi_image_free(pixels);

	MipData mip0;
	mip0.data       = tex->data.data();
	mip0.rowPitch   = static_cast<u64>(width) * 4;
	mip0.slicePitch = mip0Size;
	mip0.width      = width;
	mip0.height     = height;
	tex->mips.push_back(mip0);

	// Generate remaining mips via box filter.
	u64 offset = mip0Size;
	u32 prevW = width, prevH = height;
	for (u32 m = 1; m < mipCount; ++m)
	{
		u32 mw = std::max(1u, prevW / 2);
		u32 mh = std::max(1u, prevH / 2);
		u64 mipSize = static_cast<u64>(mw) * mh * 4;

		const u8* srcData = tex->mips[m - 1].data;
		u8* dstData = tex->data.data() + offset;
		GenerateNextMip(srcData, prevW, prevH, dstData, mw, mh);

		MipData mipEntry;
		mipEntry.data       = dstData;
		mipEntry.rowPitch   = static_cast<u64>(mw) * 4;
		mipEntry.slicePitch = mipSize;
		mipEntry.width      = mw;
		mipEntry.height     = mh;
		tex->mips.push_back(mipEntry);

		offset += mipSize;
		prevW = mw;
		prevH = mh;
	}

	LOG_DEBUG("TextureLoader: loaded '{}' — {}x{}, {} mips ({})",
	          tex->name, width, height, mipCount,
	          colorSpace == TextureColorSpace::sRGB ? "sRGB" : "linear");

	return tex;
}

// ---------------------------------------------------------------------------
// TextureLoader::Load
// ---------------------------------------------------------------------------

URef<TextureData> TextureLoader::Load(const String& path, TextureColorSpace colorSpace)
{
	ImageFileFormat fmt = DetectFormat(path);
	switch (fmt)
	{
		case ImageFileFormat::DDS: return LoadDDS(path);
		case ImageFileFormat::STB: return LoadSTB(path, colorSpace);
	}
	return nullptr;
}

std::future<URef<TextureData>> TextureLoader::LoadAsync(const String& path, ThreadPool& pool,
                                                       TextureColorSpace colorSpace)
{
	return pool.enqueue([](const String& p, TextureColorSpace cs) { return Load(p, cs); },
	                    path, colorSpace);
}

Vector<std::future<URef<TextureData>>> TextureLoader::LoadBatch(const Vector<String>& paths,
                                                               ThreadPool& pool,
                                                               TextureColorSpace colorSpace)
{
	Vector<std::future<URef<TextureData>>> futures;
	futures.reserve(paths.size());
	for (const String& p : paths)
	{
		futures.push_back(pool.enqueue([](const String& path, TextureColorSpace cs) { return Load(path, cs); },
		                               p, colorSpace));
	}
	return futures;
}
