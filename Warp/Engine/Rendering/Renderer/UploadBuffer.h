#pragma once

#include <Common/CommonTypes.h>
#include <Memory/RingBuffer.h>

class Buffer;

struct UploadAllocation
{
	void* cpuPtr;  // write your data here
	u64   gpuAddr; // bind as CBV / uniform buffer / argument buffer
	u64   offset;  // byte offset within the backing buffer
	u64   size;
};

class UploadBuffer
{
public:
	virtual ~UploadBuffer() = default;

	virtual void             Initialize(u64 size, u32 framesInFlight) = 0;
	virtual UploadAllocation Alloc(u64 size, u64 alignment = 256)     = 0;
	virtual void             OnBeginFrame()                           = 0;
	virtual void             Cleanup()                                = 0;

	// Returns the underlying Buffer so it can be used as a source for CopyBuffer.
	// Use with UploadAllocation::offset as the source offset for re-uploads.
	virtual Buffer* GetBackingBuffer() = 0;

	u64 GetTotalSize() const { return m_size; }

protected:
	RingBufferTabbed m_ringBuffer; // offset tracking is identical across all APIs
	u64              m_size = 0;
};
