#pragma once

#include <Common/CommonTypes.h>

template<class TItemType, class TContainer>
class BufferedContainer
{
public:
	TContainer& GetBackContainer() { return BufferPool[(ActiveBufferIndex + 1) % 2]; }
	const TContainer& GetBackContainer() const { return BufferPool[(ActiveBufferIndex + 1) % 2]; }

	void AddItem(const TItemType& item)
	{
		std::unique_lock<Futex> lock(BufferLock);
		BufferPool[ActiveBufferIndex] = item;
	}

	void SwapBuffer()
	{
		std::unique_lock<Futex> lock(BufferLock);
		ActiveBufferIndex ^= 1;
	}

private:
	mutable Futex BufferLock;
	Array<TContainer, 2> BufferPool;
	int ActiveBufferIndex = 0;
};