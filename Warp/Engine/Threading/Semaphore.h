#pragma once

#include <Common/CommonTypes.h>
#include <Math/MathHelper.h>

#include <mutex>
#include <condition_variable>

class Semaphore
{
public:
	Semaphore(uint currVal, uint max)
		: m_currVal(currVal)
		, m_max(max) {}


	void Wait()
	{
		std::unique_lock<std::mutex> lock(m_mtx);
		m_cv.wait(lock, [&]() {return m_currVal > 0; });
		--m_currVal;
	}

	void Signal()
	{
		std::unique_lock<std::mutex> lock(m_mtx);
		m_currVal = MathHelper::Min(++m_currVal, m_max);
		m_cv.notify_one();
	}

private:
	std::mutex m_mtx;
	std::condition_variable m_cv;

	u8 m_currVal;
	u8 m_max;
};
