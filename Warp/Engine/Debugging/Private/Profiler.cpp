#include <Debugging/Profiler.h>
#include <Debugging/Logging.h>

#include <algorithm>
#include <chrono>

namespace
{
	const std::chrono::steady_clock::time_point g_processStart = std::chrono::steady_clock::now();

	// Enough for a frame's worth of scopes. Reserved so a push never mallocs
	// while holding the lock.
	constexpr size_t k_eventReserve = 4096;
	constexpr size_t k_batchReserve = 64;
}

Profiler& Profiler::Get()
{
	static Profiler instance;
	static bool reserved = [&] {
		instance.m_events.Reserve(k_eventReserve);
		instance.m_traceQueue.Reserve(k_batchReserve);
		return true;
	}();
	(void)reserved;
	return instance;
}

Profiler::~Profiler()
{
	EndSession();
}

int64 Profiler::NowMicros()
{
	const auto delta = std::chrono::steady_clock::now() - g_processStart;
	return std::chrono::duration_cast<std::chrono::microseconds>(delta).count();
}

void Profiler::BeginSession(const String& filePath)
{
	if (m_capturing.load(std::memory_order_relaxed))
	{
		return;
	}

	m_traceFile.open(filePath, std::ios::out | std::ios::trunc);
	if (!m_traceFile)
	{
		LOG_ERROR("Profiler: could not open trace file '{}'", filePath);
		return;
	}

	m_traceFile << "{\"otherData\":{},\"traceEvents\":[";
	m_eventCount = 0;

	m_shutdownWriter.store(false, std::memory_order_relaxed);
	m_capturing.store(true, std::memory_order_release);
	m_writerThread = std::make_unique<std::thread>(&Profiler::WriterThread, this);

	LOG_DEBUG("Profiler: capturing to '{}'", filePath);
}

void Profiler::EndSession()
{
	if (!m_capturing.load(std::memory_order_acquire))
	{
		return;
	}

	// Stop queueing first so the writer sees a stable tail.
	m_capturing.store(false, std::memory_order_release);

	m_shutdownWriter.store(true, std::memory_order_release);
	m_writerWakeCV.notify_all();

	if (m_writerThread && m_writerThread->joinable())
	{
		m_writerThread->join();
	}
	m_writerThread.reset();

	// Whatever the writer did not reach before shutdown.
	DrainTraceQueue();

	m_traceFile << "]}";
	m_traceFile.close();

	LOG_DEBUG("Profiler: capture ended");
}

void Profiler::Record(const ProfileEvent& event)
{
	// Always recorded so the live stats work without a capture running.
	m_events.AddItem(event);
}

void Profiler::OnFrameEnd()
{
	m_events.SwapBuffer();
	const Vector<ProfileEvent>& events = m_events.GetBackContainer();

	// Fold per name totals for the debug UI.
	m_frameEntries.clear();
	for (const ProfileEvent& event : events)
	{
		const f64 millis = static_cast<f64>(event.endMicros - event.startMicros) / 1000.0;

		auto it = std::find_if(m_frameEntries.begin(), m_frameEntries.end(),
							   [&](const ProfileFrameEntry& entry) { return entry.name == event.name; });

		if (it == m_frameEntries.end())
		{
			m_frameEntries.push_back({ event.name, millis, 1 });
		}
		else
		{
			it->totalMillis += millis;
			++it->callCount;
		}
	}

	// Hand the batch off rather than serializing here. Copying PODs is far
	// cheaper than formatting JSON on the frame thread.
	if (m_capturing.load(std::memory_order_relaxed) && !events.empty())
	{
		m_traceQueue.AddItem(events);
		m_writerWakeCV.notify_one();
	}

	m_events.ClearBackContainer();
}

void Profiler::WriterThread()
{
	while (!m_shutdownWriter.load(std::memory_order_acquire))
	{
		{
			std::unique_lock<std::mutex> lock(m_writerWakeMutex);
			m_writerWakeCV.wait_for(lock, std::chrono::milliseconds(50));
		}

		DrainTraceQueue();
	}
}

void Profiler::DrainTraceQueue()
{
	m_traceQueue.SwapBuffer();

	for (const Vector<ProfileEvent>& batch : m_traceQueue.GetBackContainer())
	{
		WriteBatch(batch);
	}

	m_traceQueue.ClearBackContainer();
}

void Profiler::WriteBatch(const Vector<ProfileEvent>& events)
{
	if (!m_traceFile)
	{
		return;
	}

	for (const ProfileEvent& event : events)
	{
		// Chrome trace wants commas between entries, not after the last one.
		if (m_eventCount++ > 0)
		{
			m_traceFile << ',';
		}

		m_traceFile << "{\"cat\":\"function\",\"dur\":" << (event.endMicros - event.startMicros) << ",\"name\":\""
					<< event.name << "\",\"ph\":\"X\",\"pid\":0,\"tid\":" << event.threadId
					<< ",\"ts\":" << event.startMicros << '}';
	}

	// No flush per event. The stream flushes on close.
}
