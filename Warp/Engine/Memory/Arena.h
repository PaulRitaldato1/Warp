#pragma once

#include "Common/CommonTypes.h"
#include "Debugging/Assert.h"

class ArenaFrameAllocator
{
public:
	ArenaFrameAllocator(size_t size) : m_size(size), m_used(0)
	{
		m_data = static_cast<unsigned char*>(std::malloc(size));
		if (!m_data)
		{
			throw std::bad_alloc();
		}
	}

	~ArenaFrameAllocator()
	{
		if (m_data)
		{
			std::free(m_data);
			m_data = nullptr;
		}
	}

	template <typename T, typename... Args>
	T* AllocateType(Args&&... args)
	{
		size_t alignment = alignof(T);
		size_t alignedSize = (m_used + alignment - 1) & ~(alignment - 1);

		DYNAMIC_ASSERT(alignedSize + sizeof(T) <= m_size, "ArenaFrameAllocator::AllocateType: Allocation is larger than arena size");

		T* result = reinterpret_cast<T*>(m_data + alignedSize);
		used += alignedSize + sizeof(T);

		new (result) T(std::forward<Args>(args)...);

		return result;
	}

	void* Allocate(size_t size)
	{
		size_t requiredSize = m_used + size;

		DYNAMIC_ASSERT(requiredSize <= m_size, "ArenaFrameAllocator::Allocate: Allocation is larger than arena size");

		void* result = m_data + m_used;
		m_used += size;
		return result;
	}

	void Reset()
	{
		m_used = 0;
	}

	void Resize(size_t newSize)
	{

		m_used = 0;

		if (m_data)
		{
			std::free(m_data);
			m_data = nullptr;
		}

		m_data = static_cast<unsigned char*>(std::malloc(newSize));

		if (!m_data)
		{
			throw std::bad_alloc();
		}

		m_size = newSize;
	}

private:

	const size_t m_allocationGranularity = 64 * 1024; // 64 KB Pages are standard on windows systems

	size_t m_size;
	size_t m_used;
	unsigned char* m_data;
};