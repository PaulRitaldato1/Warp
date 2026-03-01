#pragma once

#include <Common/CommonTypes.h>

enum class BufferType
{
	Vertex,
	Index,
	Constant,
};

struct BufferDesc
{
	BufferType type;
	u32        numElements = 0;
	u32        stride      = 0;   // bytes per element
	String     name;
};

// Forward declare for PendingStagingUpload.
class Buffer;

// Returned by Buffer::UploadData(). Represents a staging buffer that has been
// filled with data and is ready to be copied to the destination GPU buffer.
// Hand this to the Renderer via QueueStagingUpload() — the Renderer records
// the copy command and frees the staging buffer once the GPU is done.
struct PendingStagingUpload
{
	URef<Buffer> stagingBuffer;
	Buffer*      destination = nullptr;
	u64          size        = 0;

	bool IsValid() const { return stagingBuffer != nullptr; }
};

class Buffer
{
public:
	virtual ~Buffer() = default;

	// Creates a staging buffer, copies data into it, and returns a
	// PendingStagingUpload for the Renderer to record the GPU copy.
	virtual PendingStagingUpload UploadData(const void* data, size_t size) = 0;

	// CPU-side map/unmap — only valid for internal staging buffers.
	// Asserts on GPU-only buffers.
	virtual void* Map()   = 0;
	virtual void  Unmap() = 0;

	virtual u64   GetSize() const          = 0;
	virtual void* GetNativeHandle() const  = 0;
};
