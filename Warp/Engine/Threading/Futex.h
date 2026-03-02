#pragma once

#include <Common/CommonTypes.h>

#ifdef WARP_APPLE
#include <os/lock.h>
#else
#include <atomic>
#endif

// Lightweight user-space mutex.  The fast path (no contention) is a single
// atomic CAS with no syscall.  The slow path parks the thread via the OS
// wait primitive:
//   Linux  — futex(FUTEX_WAIT / FUTEX_WAKE)
//   Windows — WaitOnAddress / WakeByAddressSingle
//   Apple  — os_unfair_lock (already a futex-style lock)
//
// Satisfies BasicLockable — works with std::unique_lock / std::scoped_lock.

class Futex
{
public:
	void Lock();
	void Unlock();

	// BasicLockable interface
	void lock()   { Lock(); }
	void unlock() { Unlock(); }

private:
#ifdef WARP_APPLE
	os_unfair_lock m_lock = OS_UNFAIR_LOCK_INIT;
#else
	// 0 = unlocked
	// 1 = locked, no waiters
	// 2 = locked, one or more threads waiting
	std::atomic<u32> m_state{0};
#endif
};
