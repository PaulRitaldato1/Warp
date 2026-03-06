#pragma once

#include <Common/CommonTypes.h>

class Buffer;
class Texture;

struct TextureMipUpload
{
	u64 srcOffset;   // byte offset into the staging buffer
	u32 mipLevel;
	u32 arraySlice;
};

struct PendingTextureUpload
{
	URef<Buffer>             stagingBuffer;
	Texture*                 destination = nullptr;
	Vector<TextureMipUpload> mips;

	bool IsValid() const { return stagingBuffer != nullptr; }
};