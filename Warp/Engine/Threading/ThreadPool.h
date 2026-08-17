#pragma once

#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>
#include <tuple>
#include <type_traits>
#include <typeinfo>

template<typename T> bool IsReady(const std::future<T> fut) { return fut.wait_for(std::chrono::seconds(0)) == std::future_status::ready; }

/* ThreadPool class */
class ThreadPool
{
public:

	ThreadPool()
	{
		m_shutdown.store(false, std::memory_order_relaxed);
		createThreads(1);
	}

	ThreadPool(std::size_t numThreads)
	{
		m_shutdown.store(false, std::memory_order_relaxed);
		createThreads(numThreads);
	}

	~ThreadPool()
	{
		{
			std::scoped_lock<std::mutex> lock(m_jobMutex);
			m_shutdown.store(true, std::memory_order_relaxed);
		}
		m_notifier.notify_all();

		for (std::thread& th : m_threads)
		{
			th.join();
		}
	}

	using Job = std::function<void()>;

	// Enqueue a job with no result. Skips the packaged_task and shared state that
	// enqueue() allocates, which matters for coroutine resumes.
	void Post(Job job)
	{
		{
			std::scoped_lock<std::mutex> lock(m_jobMutex);
			m_jobQueue.push(std::move(job));
		}
		m_notifier.notify_one();
	}

	//add any arg # function to queue
	template <typename Func, typename... Args>
	auto enqueue(Func&& f, Args&&... args)
	{
		//get return type of the function
		using RetType = std::invoke_result_t<Func, Args...>;

		// Capture by value — args must be copied/moved into the lambda because
		// enqueue returns before the worker thread executes the task.
		auto task = std::make_shared<std::packaged_task<RetType()>>(
			[f = std::forward<Func>(f),
			 boundArgs = std::make_tuple(std::forward<Args>(args)...)]() mutable {
				return std::apply(std::move(f), std::move(boundArgs));
			});

		{
			std::scoped_lock<std::mutex> lock(m_jobMutex);

			//place the job into the queue
			m_jobQueue.emplace([task]() {
				(*task)();
				});
		}
		m_notifier.notify_one();

		return task->get_future();
	}

	/* utility functions */
	std::size_t getThreadCount() const
	{
		return m_threads.size();
	}

private:

	std::vector<std::thread> m_threads;
	std::queue<Job> m_jobQueue;
	std::condition_variable m_notifier;
	std::mutex m_jobMutex;
	std::atomic<bool> m_shutdown;

	void createThreads(std::size_t numThreads)
	{
		m_threads.reserve(numThreads);
		for (std::size_t i = 0; i != numThreads; ++i)
		{
			m_threads.emplace_back(std::thread([this]()
				{
					while (true)
					{
						Job job;

						{
							std::unique_lock<std::mutex> lock(m_jobMutex);
							m_notifier.wait(lock, [this] { return !m_jobQueue.empty() || m_shutdown.load(std::memory_order_relaxed); });

							// Drain the queue before exiting. Dropping a queued job leaks
							// the coroutine frame whose resume it was carrying.
							if (m_jobQueue.empty())
							{
								if (m_shutdown.load(std::memory_order_relaxed))
								{
									break;
								}
								continue;
							}

							job = std::move(m_jobQueue.front());

							m_jobQueue.pop();
						}
						job();
					}
				}));
		}
	}
}; /* end ThreadPool Class */