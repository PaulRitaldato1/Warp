#include <Threading/Futex.h>

// ---------------------------------------------------------------------------
// Apple — os_unfair_lock (already a futex-style user-space lock)
// ---------------------------------------------------------------------------
#ifdef WARP_APPLE

void Futex::Lock()
{
	os_unfair_lock_lock(&m_lock);
}

void Futex::Unlock()
{
	os_unfair_lock_unlock(&m_lock);
}

// ---------------------------------------------------------------------------
// Linux — futex syscall
// ---------------------------------------------------------------------------
#elif defined(WARP_LINUX)

#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

static void FutexWait(std::atomic<u32>& futexWord, u32 expectedValue)
{
	syscall(SYS_futex, reinterpret_cast<u32*>(&futexWord),
	        FUTEX_WAIT | FUTEX_PRIVATE_FLAG, expectedValue,
	        nullptr, nullptr, 0);
}

static void FutexWake(std::atomic<u32>& futexWord, u32 count)
{
	syscall(SYS_futex, reinterpret_cast<u32*>(&futexWord),
	        FUTEX_WAKE | FUTEX_PRIVATE_FLAG, count,
	        nullptr, nullptr, 0);
}

void Futex::Lock()
{
	// Fast path: uncontested lock.
	u32 expected = 0;
	if (m_state.compare_exchange_strong(expected, 1, std::memory_order_acquire))
		return;

	// Slow path: another thread holds the lock.
	// Spin briefly before parking — helps if the lock holder is about to release.
	for (u32 spin = 0; spin < 40; ++spin)
	{
		expected = 0;
		if (m_state.compare_exchange_weak(expected, 1, std::memory_order_acquire))
			return;
	}

	// Park: set state to 2 (locked-with-waiters) and wait.
	while (true)
	{
		u32 prev = m_state.exchange(2, std::memory_order_acquire);
		if (prev == 0)
			return; // We acquired the lock (was unlocked between exchange and here).

		FutexWait(m_state, 2);
	}
}

void Futex::Unlock()
{
	u32 prev = m_state.exchange(0, std::memory_order_release);
	if (prev == 2)
	{
		// There were waiters — wake one.
		FutexWake(m_state, 1);
	}
}

// ---------------------------------------------------------------------------
// Windows — WaitOnAddress / WakeByAddressSingle
// ---------------------------------------------------------------------------
#elif defined(WARP_WINDOWS)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

void Futex::Lock()
{
	// Fast path: uncontested lock.
	u32 expected = 0;
	if (m_state.compare_exchange_strong(expected, 1, std::memory_order_acquire))
		return;

	// Spin briefly before parking.
	for (u32 spin = 0; spin < 40; ++spin)
	{
		expected = 0;
		if (m_state.compare_exchange_weak(expected, 1, std::memory_order_acquire))
			return;
	}

	// Park: set state to 2 (locked-with-waiters) and wait.
	while (true)
	{
		u32 prev = m_state.exchange(2, std::memory_order_acquire);
		if (prev == 0)
			return;

		u32 waitValue = 2;
		WaitOnAddress(reinterpret_cast<volatile void*>(&m_state),
		              &waitValue, sizeof(u32), INFINITE);
	}
}

void Futex::Unlock()
{
	u32 prev = m_state.exchange(0, std::memory_order_release);
	if (prev == 2)
	{
		WakeByAddressSingle(reinterpret_cast<void*>(&m_state));
	}
}

#endif
