#pragma once

#include <Common/CommonTypes.h>

#include <coroutine>
#include <optional>
#include <utility>

// Lazy coroutine task. The body does not run until the task is awaited.
// Completion tail-calls into the awaiting coroutine via symmetric transfer, so a
// chain of awaits does not grow the stack.
//
// Errors are expected to travel as values (see LoadError), so an escaping exception
// is treated as a bug rather than stored.
template <typename T> class Task
{
public:
	struct promise_type;
	using Handle = std::coroutine_handle<promise_type>;

	struct FinalAwaiter
	{
		bool await_ready() const noexcept
		{
			return false;
		}

		std::coroutine_handle<> await_suspend(Handle handle) const noexcept
		{
			std::coroutine_handle<> continuation = handle.promise().continuation;
			return continuation ? continuation : std::noop_coroutine();
		}

		void await_resume() const noexcept {}
	};

	struct promise_type
	{
		Task get_return_object()
		{
			return Task(Handle::from_promise(*this));
		}

		std::suspend_always initial_suspend() noexcept
		{
			return {};
		}
		FinalAwaiter final_suspend() noexcept
		{
			return {};
		}

		void return_value(T value)
		{
			result = std::move(value);
		}
		void unhandled_exception()
		{
			std::terminate();
		}

		std::coroutine_handle<> continuation;
		std::optional<T>		result;
	};

	Task() = default;
	explicit Task(Handle handle) : m_handle(handle) {}

	Task(Task&& other) noexcept : m_handle(std::exchange(other.m_handle, {})) {}

	Task& operator=(Task&& other) noexcept
	{
		if (this != &other)
		{
			if (m_handle)
			{
				m_handle.destroy();
			}
			m_handle = std::exchange(other.m_handle, {});
		}
		return *this;
	}

	Task(const Task&)			 = delete;
	Task& operator=(const Task&) = delete;

	~Task()
	{
		if (m_handle)
		{
			m_handle.destroy();
		}
	}

	bool IsDone() const
	{
		return !m_handle || m_handle.done();
	}

	auto operator co_await() && noexcept
	{
		struct Awaiter
		{
			Handle handle;

			bool await_ready() const noexcept
			{
				return !handle || handle.done();
			}

			// Returning the handle both starts the lazy task and transfers control
			// to it in a single tail call.
			std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting) const noexcept
			{
				handle.promise().continuation = awaiting;
				return handle;
			}

			T await_resume() const
			{
				return std::move(*handle.promise().result);
			}
		};

		return Awaiter{ m_handle };
	}

private:
	Handle m_handle{};
};

// Same as above with no result value.
template <> class Task<void>
{
public:
	struct promise_type;
	using Handle = std::coroutine_handle<promise_type>;

	struct FinalAwaiter
	{
		bool await_ready() const noexcept
		{
			return false;
		}

		std::coroutine_handle<> await_suspend(Handle handle) const noexcept
		{
			std::coroutine_handle<> continuation = handle.promise().continuation;
			return continuation ? continuation : std::noop_coroutine();
		}

		void await_resume() const noexcept {}
	};

	struct promise_type
	{
		Task get_return_object()
		{
			return Task(Handle::from_promise(*this));
		}

		std::suspend_always initial_suspend() noexcept
		{
			return {};
		}
		FinalAwaiter final_suspend() noexcept
		{
			return {};
		}

		void return_void() noexcept {}
		void unhandled_exception()
		{
			std::terminate();
		}

		std::coroutine_handle<> continuation;
	};

	Task() = default;
	explicit Task(Handle handle) : m_handle(handle) {}

	Task(Task&& other) noexcept : m_handle(std::exchange(other.m_handle, {})) {}

	Task& operator=(Task&& other) noexcept
	{
		if (this != &other)
		{
			if (m_handle)
			{
				m_handle.destroy();
			}
			m_handle = std::exchange(other.m_handle, {});
		}
		return *this;
	}

	Task(const Task&)			 = delete;
	Task& operator=(const Task&) = delete;

	~Task()
	{
		if (m_handle)
		{
			m_handle.destroy();
		}
	}

	bool IsDone() const
	{
		return !m_handle || m_handle.done();
	}

	auto operator co_await() && noexcept
	{
		struct Awaiter
		{
			Handle handle;

			bool await_ready() const noexcept
			{
				return !handle || handle.done();
			}

			std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting) const noexcept
			{
				handle.promise().continuation = awaiting;
				return handle;
			}

			void await_resume() const noexcept {}
		};

		return Awaiter{ m_handle };
	}

private:
	Handle m_handle{};
};

// Fire-and-forget root for work nobody awaits. Starts eagerly and destroys its own
// frame on completion, so the caller holds nothing and must track liveness by other
// means if it needs to (see ResourceManager's in-flight counter).
struct DetachedTask
{
	struct promise_type
	{
		DetachedTask get_return_object() noexcept
		{
			return {};
		}

		std::suspend_never initial_suspend() noexcept
		{
			return {};
		}
		std::suspend_never final_suspend() noexcept
		{
			return {};
		}

		void return_void() noexcept {}
		void unhandled_exception()
		{
			std::terminate();
		}
	};
};
