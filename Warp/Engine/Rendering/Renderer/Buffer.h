#pragma once

#include <Common/CommonTypes.h>

enum class BufferType
{
	Vertex,
	Index,
	Constant,
};

enum class BufferUsage
{
	// Uploaded once; lives on GPU-only memory (D3D12 default heap).
	// Optimal for vertex/index data that never changes.
	Static,

	// Updated every frame or multiple times per frame (D3D12 upload heap).
	// Use for constant buffers, dynamic geometry, CPU-written data.
	Dynamic,
};

struct BufferDesc
{
	BufferType  type;
	u32         numElements  = 0;
	u32         stride       = 0;         // bytes per element
	const void* initialData  = nullptr;   // optional, copied during creation
	BufferUsage usage        = BufferUsage::Static;
	String      name;
};

class Buffer
{
public:
	virtual ~Buffer() = default;

	// Copy data into the buffer.
	// For Dynamic buffers this is a direct CPU write.
	// For Static buffers this initiates an upload; a command list must flush the copy.
	virtual void  UploadData(const void* data, size_t size) = 0;

	// CPU-side map/unmap — valid only for Dynamic buffers.
	virtual void* Map()   = 0;
	virtual void  Unmap() = 0;

	virtual u64   GetSize() const          = 0;
	virtual void* GetNativeHandle() const  = 0;

	bool IsDynamic() const { return m_usage == BufferUsage::Dynamic; }

protected:
	BufferUsage m_usage = BufferUsage::Static;
};
