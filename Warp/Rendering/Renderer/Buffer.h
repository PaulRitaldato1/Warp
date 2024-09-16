#pragma once

#include <Common/CommonTypes.h>

class IDevice;
class IResource;

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

class StaticBufferHeap
{
public:
	
	~StaticBufferHeap()
	{
		Destroy();
	}

	void Create(IDevice* device, BufferType type, u32 size, bool useVRAM, const char* name);
	void Destroy();

	bool AllocBuffer(u32 numElements, u32 strideInBytes, const void* initData);
};

class DynamicBufferHeap
{

};