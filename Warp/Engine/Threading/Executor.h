#pragma once

#include <Common/CommonTypes.h>
#include <Threading/ThreadPool.h>

#include <coroutine>
#include <mutex>

// Where a coroutine resumes. Kept as an interface so a task can be retargeted at a
// different thread without touching the task body.
class Executor
{
public:
	virtual ~Executor()									  = default;
	virtual void Schedule(std::coroutine_handle<> handle) = 0;
};

// Resumes on any thread pool worker.
class ThreadPoolExecutor : public Executor
{
public:
	explicit ThreadPoolExecutor(ThreadPool& pool) : m_pool(pool) {}

	void Schedule(std::coroutine_handle<> handle) override
	{
		m_pool.Post([handle] { handle.resume(); });
	}

private:
	ThreadPool& m_pool;
};

// Queues resumes for one specific thread. Drain() must be called from that thread.
// Used for work that must run where the GPU device lives.
class SerialExecutor : public Executor
{
public:
	void Schedule(std::coroutine_handle<> handle) override
	{
		std::scoped_lock<std::mutex> lock(m_mutex);
		m_pending.push_back(handle);
	}

	void Drain()
	{
		Vector<std::coroutine_handle<>> batch;
		{
			std::scoped_lock<std::mutex> lock(m_mutex);
			batch.swap(m_pending);
		}

		// Resume outside the lock. A resumed coroutine may Schedule() again.
		for (std::coroutine_handle<> handle : batch)
		{
			handle.resume();
		}
	}

	bool HasPending() const
	{
		std::scoped_lock<std::mutex> lock(m_mutex);
		return !m_pending.empty();
	}

private:
	mutable std::mutex				m_mutex;
	Vector<std::coroutine_handle<>> m_pending;
};

// co_await ResumeOn{ executor } continues on that executor's thread.
struct ResumeOn
{
	Executor& executor;

	bool await_ready() const noexcept
	{
		return false;
	}

	void await_suspend(std::coroutine_handle<> handle) const
	{
		executor.Schedule(handle);
	}

	void await_resume() const noexcept {}
};

// Resumes coroutines once the GPU copy carrying their upload has actually retired.
// A coroutine parks after queueing its upload; the batch is tagged with a fence
// value when that frame's copy list is submitted, and resumes once the copy queue
// reports that value complete. Single threaded: all three calls share a thread.
class UploadFenceScheduler
{
public:
	void Schedule(std::coroutine_handle<> handle)
	{
		m_pending.push_back(handle);
	}

	// Called once the frame's copy list has been submitted.
	void OnSubmitted(u64 fenceValue)
	{
		if (m_pending.empty())
		{
			return;
		}

		m_batches.push_back({ std::move(m_pending), fenceValue });
		m_pending.clear();
	}

	void Poll(u64 completedValue)
	{
		// Collect before resuming: a resumed coroutine may Schedule() again, and
		// that must not disturb the container being iterated.
		Vector<std::coroutine_handle<>> ready;
		for (size_t i = 0; i < m_batches.size();)
		{
			if (m_batches[i].fenceValue <= completedValue)
			{
				for (std::coroutine_handle<> handle : m_batches[i].handles)
				{
					ready.push_back(handle);
				}
				m_batches[i] = std::move(m_batches.back());
				m_batches.pop_back();
			}
			else
			{
				++i;
			}
		}

		for (std::coroutine_handle<> handle : ready)
		{
			handle.resume();
		}
	}

	// Releases every waiter regardless of fence state. Shutdown only, where no
	// further copies will ever be submitted.
	void ReleaseAll()
	{
		OnSubmitted(0);
		Poll(~0ull);
	}

	bool HasPending() const
	{
		return !m_pending.empty() || !m_batches.empty();
	}

private:
	struct Batch
	{
		Vector<std::coroutine_handle<>> handles;
		u64								fenceValue;
	};

	Vector<std::coroutine_handle<>> m_pending;
	Vector<Batch>					m_batches;
};

// co_await UploadComplete{ scheduler } resumes when the queued upload's copy retires.
struct UploadComplete
{
	UploadFenceScheduler& scheduler;

	bool await_ready() const noexcept
	{
		return false;
	}

	void await_suspend(std::coroutine_handle<> handle) const
	{
		scheduler.Schedule(handle);
	}

	void await_resume() const noexcept {}
};
