#pragma once
#include <Common/CommonTypes.h>

class RingBuffer
{
public:
	RingBuffer()
	{
		m_head = 0;
		m_allocatedSize = 0;
		m_totalSize = 0;
	}

	RingBuffer(const u32 size) : m_head(0), m_allocatedSize(0), m_totalSize(size) {}
	
	~RingBuffer();

	void Create(const u32 size);

	inline u32 Size() const { return m_allocatedSize; }
	inline u32 Tail() const { return (m_allocatedSize + m_head) % m_totalSize; }
	inline u32 Head() const { return m_head; }

	bool Alloc(const u32 size, u32* outData);
	bool Free(const u32 size);

	u32 PaddingToBufferEnd(const u32 size) const;

private:

	u32 m_head;
	u32 m_allocatedSize;
	u32 m_totalSize;
};

//This is an inner buffer of the RingBuffer above. The number of tabs correspond to the 
// number of backbuffers, where each one gets a portion of the RingBuffer
//			RingBuffer
// ******************************
//   Tab   |    Tab	  |   Tab   |
class RingBufferTabbed
{
public:

	RingBufferTabbed() = default;

	RingBufferTabbed(u32 numBackbuffers, u32 size) : m_ringBuffer(size)
	{
		m_memAllocatedPerBackBuffer = Vector<u32>(numBackbuffers);
	}

	~RingBufferTabbed();

	void Create(const u32 numBackBuffers, const u32 memTotalSize);
	void Destroy();

	bool Alloc(const u32 size, u32* outData);
	
	void OnBeginFrame();

private:

	RingBuffer m_ringBuffer;

	u32 m_backBufferIndex = 0;

	u32 m_memAllocatedInFrame = 0;
	Vector<u32> m_memAllocatedPerBackBuffer;
};