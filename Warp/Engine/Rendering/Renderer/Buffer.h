#pragma once

#include <Common/CommonTypes.h>

class IDevice;
class IResource;
class ICommandList;

enum BufferType
{
	BufferType_Vertex = 0,
	BufferType_Index,
	BufferType_Constant,

	NUM_BUFFER_TYPES
};

struct BufferDesc
{
	BufferType Type;
	u32 NumElements;
	u32 Stride;
	const void* Data;
	String Name;
};

struct Vertex
{
	float Pos[3];
	float Color[3];
	float Normal[3];
	float Tangent[3];
	float UV[2];
};

enum class VertexBufferType
{
	COLOR = 0,
	COLOR_AND_ALPHA,
	NORMAL,
	NORMAL_AND_TANGENT,

	NUM_VERTEX_BUFFER_TYPES
};

class IBuffer
{
public:
	virtual ~IBuffer() = default;

	// virtual void Create(IDevice* device, BufferType type, u32 size, bool useVRAM, const char* name) = 0;

	// virtual u64 GetSize()		 = 0;
	// virtual void* GetNativePtr() = 0;
	// virtual bool IsDynamic()	 = 0;

	// virtual bool AllocBuffer(u32 numElements, u32 strideInBytes, const void* initData) = 0;
	virtual void UploadData(URef<ICommandList> commandList) = 0;
	virtual void* Map()										= 0;
	virtual void UnMap()									= 0;

private:
	// Static: Used for infrequently updated data like vertex/index buffers that do not update frequently
	// Dynamic: Used for data that will be updated per-frame or multiple times per frame
	bool m_bIsDynamic;
};
