#pragma once

#include <Common/CommonTypes.h>
#include <Rendering/Renderer/UploadBuffer.h>

class Texture;

struct TextureMipUpload
{
	u64 srcOffset;    // byte offset into the staging buffer
	u32 srcRowPitch;  // bytes per row in the staging buffer (256-byte aligned for D3D12)
	u32 mipLevel;
	u32 arraySlice;
};

struct PendingTextureUpload
{
	// Owns the upload-heap memory for this texture's staging data.
	// Freed once the GPU copy fence completes (tracked in Renderer).
	URef<UploadBuffer>       stagingUploadBuffer;
	Texture*                 destination = nullptr;
	Vector<TextureMipUpload> mips;

	bool IsValid() const { return stagingUploadBuffer != nullptr; }
};