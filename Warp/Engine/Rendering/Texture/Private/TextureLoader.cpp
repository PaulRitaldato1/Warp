#include <Rendering/Texture/TextureLoader.h>
#include <Threading/ThreadPool.h>
#include <Debugging/Logging.h>

#define TINYDDSLOADER_IMPLEMENTATION
#include <tinyddsloader.h>

#include <filesystem>
#include <fstream>
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
// TextureLoader::Load
// ---------------------------------------------------------------------------

URef<TextureData> TextureLoader::Load(const String& path)
{
	namespace fs = std::filesystem;
	using namespace tinyddsloader;

	DDSFile dds;
	const auto result = dds.Load(path.c_str());

	if (result != tinyddsloader::Success)
	{
		LOG_ERROR("TextureLoader: failed to load '" + path + "'");
		return nullptr;
	}

	const TextureFormat format = TranslateDXGIFormat(dds.GetFormat());
	if (format == TextureFormat::Unknown)
	{
		LOG_ERROR("TextureLoader: unsupported DXGI format in '" + path + "'");
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
			if (imageData) { totalBytes += imageData->m_memSlicePitch; }
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
			if (!src) { continue; }

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

	LOG_DEBUG("TextureLoader: loaded '" + tex->name + "' — " +
			  std::to_string(tex->width) + "x" + std::to_string(tex->height) +
			  ", " + std::to_string(tex->mipLevels) + " mips" +
			  (tex->isCubemap ? ", cubemap" : ""));

	return tex;
}

std::future<URef<TextureData>> TextureLoader::LoadAsync(const String& path, ThreadPool& pool)
{
	return pool.enqueue(Load, path);
}

Vector<std::future<URef<TextureData>>> TextureLoader::LoadBatch(const Vector<String>& paths,
																  ThreadPool& pool)
{
	Vector<std::future<URef<TextureData>>> futures;
	futures.reserve(paths.size());
	for (const auto& path : paths)
	{
		futures.push_back(pool.enqueue(Load, path));
	}
	return futures;
}
