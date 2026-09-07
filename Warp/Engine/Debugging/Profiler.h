#pragma once

#include <Common/CommonTypes.h>
#include <Threading/BufferedContainer.h>

#include <atomic>
#include <condition_variable>
#include <fstream>
#include <thread>

// Override from CMake with -DPROFILING_ENABLE=0 to compile the macros out
// entirely. Off in release regardless.
#ifndef PROFILING_ENABLE
#define PROFILING_ENABLE 1
#endif

#ifdef WARP_RELEASE
#undef PROFILING_ENABLE
#define PROFILING_ENABLE 0
#endif

// Scope profiler. Emits Chrome trace JSON for chrome://tracing or Perfetto.
// Events are written on a background thread, so a scope costs a timestamp pair
// and a push. Names are not copied, so they must be string literals.

struct ProfileEvent
{
	const char* name = nullptr;
	int64 startMicros	 = 0;
	int64 endMicros	 = 0;
	u32 threadId	 = 0;
};

// Per name totals for the current frame, for the debug UI.
struct ProfileFrameEntry
{
	const char* name  = nullptr;
	f64 totalMillis	  = 0.0;
	u32 callCount	  = 0;
};

class WARP_API Profiler
{
public:
	Profiler(const Profiler&)			 = delete;
	Profiler& operator=(const Profiler&) = delete;

	static Profiler& Get();

	// Starts the writer thread. Live stats work without a session.
	void BeginSession(const String& filePath = "profile.json");
	void EndSession();

	bool IsCapturing() const
	{
		return m_capturing.load(std::memory_order_relaxed);
	}

	// Called by ProfileScope. One Futex guarded push of a POD.
	void Record(const ProfileEvent& event);

	// Folds the frame's events into per name totals and hands them to the writer
	// when capturing. Call once per frame.
	void OnFrameEnd();

	// Last frame's totals, for the render stats panel.
	const Vector<ProfileFrameEntry>& GetFrameEntries() const
	{
		return m_frameEntries;
	}

	// Microseconds since process start. Keeps trace timestamps readable.
	static int64 NowMicros();

private:
	Profiler() = default;
	~Profiler();

	void WriterThread();
	void DrainTraceQueue();
	void WriteBatch(const Vector<ProfileEvent>& events);

	// Scopes push here. OnFrameEnd drains it on the frame thread.
	BufferedContainer<ProfileEvent, Vector<ProfileEvent>> m_events;

	// Whole frames handed to the writer. Only fed while capturing.
	BufferedContainer<Vector<ProfileEvent>, Vector<Vector<ProfileEvent>>> m_traceQueue;

	URef<std::thread> m_writerThread;
	std::mutex m_writerWakeMutex; // Paired with CV for wake signaling only.
	std::condition_variable m_writerWakeCV;
	std::atomic<bool> m_shutdownWriter{ false };
	std::atomic<bool> m_capturing{ false };

	std::ofstream m_traceFile;
	u32 m_eventCount = 0; // Trace JSON needs a comma between entries, not after.

	Vector<ProfileFrameEntry> m_frameEntries;
};

#if PROFILING_ENABLE

class ProfileScope
{
public:
	explicit ProfileScope(const char* name) : m_name(name), m_start(Profiler::NowMicros())
	{
	}

	~ProfileScope()
	{
		Profiler::Get().Record({ m_name, m_start, Profiler::NowMicros(), CurrentThreadId() });
	}

	ProfileScope(const ProfileScope&)			 = delete;
	ProfileScope& operator=(const ProfileScope&) = delete;

private:
	// Hashing thread::id is not free, so do it once per thread.
	static u32 CurrentThreadId()
	{
		static thread_local u32 id = static_cast<u32>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
		return id;
	}

	const char* m_name;
	int64 m_start;
};

// Two levels so __LINE__ expands before pasting. Pasting directly produces a
// literal profileScope__LINE__ and collides on the second use in a scope.
#define PROFILE_CONCAT_IMPL(a, b) a##b
#define PROFILE_CONCAT(a, b)	  PROFILE_CONCAT_IMPL(a, b)

#define PROFILE_SCOPE(name) ProfileScope PROFILE_CONCAT(profileScope, __LINE__)(name)
#define PROFILE_FUNCTION()	PROFILE_SCOPE(__FUNCTION__)

#else

#define PROFILE_SCOPE(name) \
	do                      \
	{                       \
	} while (0)
#define PROFILE_FUNCTION() \
	do                     \
	{                      \
	} while (0)

#endif
