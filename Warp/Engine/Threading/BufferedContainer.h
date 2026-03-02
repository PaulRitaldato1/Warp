#pragma once

#include <Common/CommonTypes.h>
#include <Threading/Futex.h>

// Double-buffered container for producer/consumer patterns across threads.
// The producer calls AddItem() on the active (front) buffer.
// The consumer calls SwapBuffer(), then reads from GetBackContainer().
//
// TItemType — the element type pushed into the container.
// TContainer — the container type (e.g. Vector<TItemType>).
template<class TItemType, class TContainer>
class BufferedContainer
{
public:
	// Returns the back buffer (the one NOT being written to).
	// Consumer reads from this after SwapBuffer().
	TContainer& GetBackContainer() { return m_bufferPool[(m_activeIndex + 1) % 2]; }
	const TContainer& GetBackContainer() const { return m_bufferPool[(m_activeIndex + 1) % 2]; }

	// Returns the front buffer (the one being written to).
	TContainer& GetFrontContainer() { return m_bufferPool[m_activeIndex]; }
	const TContainer& GetFrontContainer() const { return m_bufferPool[m_activeIndex]; }

	// Pushes an item into the active (front) buffer.
	void AddItem(const TItemType& item)
	{
		std::unique_lock<Futex> lock(m_lock);
		m_bufferPool[m_activeIndex].push_back(item);
	}

	void AddItem(TItemType&& item)
	{
		std::unique_lock<Futex> lock(m_lock);
		m_bufferPool[m_activeIndex].push_back(std::move(item));
	}

	// Swaps front and back buffers.
	// After this call, the old front becomes the back (consumer can read it),
	// and the old back becomes the new front (producer writes to it).
	void SwapBuffer()
	{
		std::unique_lock<Futex> lock(m_lock);
		m_activeIndex ^= 1;
	}

	// Clears the back buffer. Call after the consumer is done reading.
	void ClearBackContainer()
	{
		std::unique_lock<Futex> lock(m_lock);
		m_bufferPool[(m_activeIndex + 1) % 2].clear();
	}

private:
	Futex m_lock;
	Array<TContainer, 2> m_bufferPool;
	int m_activeIndex = 0;
};
