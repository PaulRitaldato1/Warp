#include <Debugging/Logging.h>
#include <chrono>
#include <cstdio>

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------

Logger& Logger::Get()
{
	static Logger instance;
	return instance;
}

Logger::~Logger()
{
	Shutdown();
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void Logger::Init(const String& logFilePath)
{
	if (m_initialized)
		return;

	m_logFile.open(logFilePath, std::ios::out | std::ios::trunc);
	if (m_logFile.is_open())
	{
		m_fileEnabled = true;

		m_shutdownWriter.store(false, std::memory_order_relaxed);
		m_writerThread = std::make_unique<std::thread>(&Logger::FileWriterThread, this);
	}

	m_initialized = true;
}

void Logger::Shutdown()
{
	if (!m_initialized)
		return;

	// Signal the writer thread to stop and wake it.
	m_shutdownWriter.store(true, std::memory_order_release);
	m_writerWakeCV.notify_one();

	if (m_writerThread && m_writerThread->joinable())
	{
		m_writerThread->join();
	}
	m_writerThread.reset();

	// Flush any remaining messages in the front buffer.
	m_fileBuffer.SwapBuffer();
	const Vector<String>& remaining = m_fileBuffer.GetBackContainer();
	for (const String& line : remaining)
	{
		m_logFile << line << '\n';
	}
	m_logFile.flush();
	m_logFile.close();

	m_fileEnabled = false;
	m_initialized = false;
}

// ---------------------------------------------------------------------------
// Core Log
// ---------------------------------------------------------------------------

void Logger::Log(LogLevel level, const char* file, int32 line, const String& message)
{
	const char* fileName = StripPath(file);
	String timestamp	 = FormatTimestamp();
	u8 levelIdx			 = static_cast<u8>(level);

	String formatted =
		std::format("[{}] [{}] [{}:{}] {}", timestamp, k_levelStrings[levelIdx], fileName, line, message);

	if (m_consoleEnabled)
	{
		ConsoleWrite(level, formatted);
	}

	if (m_fileEnabled)
	{
		m_fileBuffer.AddItem(std::move(formatted));
		m_writerWakeCV.notify_one();
	}
}

// ---------------------------------------------------------------------------
// Console
// ---------------------------------------------------------------------------

void Logger::ConsoleWrite(LogLevel level, const String& formatted)
{
	u8 levelIdx = static_cast<u8>(level);
	// ANSI color prefix + message + reset + newline.
	std::fprintf(stderr, "%s%s\033[0m\n", k_levelColors[levelIdx], formatted.c_str());
}

// ---------------------------------------------------------------------------
// File writer thread
// ---------------------------------------------------------------------------

void Logger::FileWriterThread()
{
	while (true)
	{
		// Wait until notified, timed out, or shutdown requested.
		// Using a predicate ensures notify_one() is never missed — if shutdown is
		// set before the thread re-enters wait_for, it returns immediately instead
		// of sleeping another 100ms.
		bool shutdown = false;
		{
			std::unique_lock<std::mutex> lock(m_writerWakeMutex);
			m_writerWakeCV.wait_for(lock, std::chrono::milliseconds(100),
				[this] { return m_shutdownWriter.load(std::memory_order_acquire); });
			shutdown = m_shutdownWriter.load(std::memory_order_acquire);
		}

		// Swap and drain the back buffer.
		m_fileBuffer.SwapBuffer();
		const Vector<String>& batch = m_fileBuffer.GetBackContainer();

		for (const String& line : batch)
		{
			m_logFile << line << '\n';
		}

		if (!batch.empty())
		{
			m_logFile.flush();
		}

		m_fileBuffer.ClearBackContainer();

		if (shutdown)
			break;
	}
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

String Logger::FormatTimestamp() const
{
	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	std::time_t tt							  = std::chrono::system_clock::to_time_t(now);
	std::tm tm								  = *std::localtime(&tt);

	std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

	char buf[16];
	std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03d", tm.tm_hour, tm.tm_min, tm.tm_sec,
				  static_cast<int>(ms.count()));
	return String(buf);
}

const char* Logger::StripPath(const char* path)
{
	const char* lastSlash = path;
	for (const char* p = path; *p != '\0'; ++p)
	{
		if (*p == '/' || *p == '\\')
			lastSlash = p + 1;
	}
	return lastSlash;
}
