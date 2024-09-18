#include "RingBuffer.h"

// ===== RingBuffer Start =====
RingBuffer::~RingBuffer()
{
	Free(m_totalSize);
}

void RingBuffer::Create(const u32 size)
{
	m_head = 0;
	m_allocatedSize = 0;
	m_totalSize = size;
}

bool RingBuffer::Alloc(const u32 size, u32* outData)
{
	if (m_allocatedSize + size <= m_totalSize)
	{
		if (outData)
		{
			*outData = Tail();
		}

		m_allocatedSize += size;
		return true;
	}

	return false;
}

bool RingBuffer::Free(const u32 size)
{
	if (m_allocatedSize > size)
	{
		m_head = (m_head + size) % m_totalSize;
		m_allocatedSize -= size;
		return true;
	}

	return false;
}

u32 RingBuffer::PaddingToBufferEnd(const u32 size) const
{
	u32 tail = Tail();
	
	if ((tail + size) > m_totalSize)
	{
		return m_totalSize - tail;
	}
	else
	{
		return 0;
	}
}

// ===== RingBuffer End =====

// ===== RingBufferTabbed Start =====
RingBufferTabbed::~RingBufferTabbed()
{
	Destroy();
}

void RingBufferTabbed::Create(const u32 numBackBuffers, const u32 memTotalSize)
{
	m_backBufferIndex = 0;
	m_memAllocatedPerBackBuffer = Vector<u32>(numBackBuffers);
	m_ringBuffer = RingBuffer(memTotalSize);
}

void RingBufferTabbed::Destroy()
{
	m_ringBuffer.Free(m_ringBuffer.Size());
}

bool RingBufferTabbed::Alloc(const uint32_t size, uint32_t* outData)
{
	u32 padding = m_ringBuffer.PaddingToBufferEnd(size);
	if (padding > 0)
	{
		m_memAllocatedInFrame += padding;
		if (m_ringBuffer.Alloc(padding, nullptr) == false)
		{
			return false;
		}
	}

	if (m_ringBuffer.Alloc(size, outData))
	{
		m_memAllocatedInFrame += size;
		return true;
	}

	return false;
}

void RingBufferTabbed::OnBeginFrame() 
{
	m_memAllocatedPerBackBuffer[m_backBufferIndex] = m_memAllocatedInFrame;
	m_memAllocatedInFrame = 0;
	
	m_backBufferIndex = (m_backBufferIndex + 1) % m_memAllocatedPerBackBuffer.size();

	u32 memToFree = m_memAllocatedPerBackBuffer[m_backBufferIndex];
	m_ringBuffer.Free(memToFree);
}