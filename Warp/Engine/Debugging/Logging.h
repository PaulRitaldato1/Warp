#pragma once

#include <Common/CommonTypes.h>
#include <Threading/BufferedContainer.h>
#include <format>
#include <fstream>
#include <thread>
#include <condition_variable>
#include <atomic>

// Compile-time log level gates.
#define LOG_WARN_ENABLED 1
#define LOG_INFO_ENABLED 1
#define LOG_DEBUG_ENABLED 1

#ifdef WARP_RELEASE
#undef LOG_DEBUG_ENABLED
#define LOG_DEBUG_ENABLED 0
#endif

enum class LogLevel : u8
{
	Debug = 0,
	Info,
	Warning,
	Error,

	Count
};

class WARP_API Logger
{
public:
	Logger(const Logger&)            = delete;
	Logger& operator=(const Logger&) = delete;

	static Logger& Get();

	// Call from WarpEngine::Init / Shutdown.
	void Init(const String& logFilePath = "warp.log");
	void Shutdown();

	// Core log function — formats and dispatches to console + file.
	void Log(LogLevel level, const char* file, int32 line, const String& message);

	// Format overload — builds the message string via std::format, then calls core Log.
	template<typename... Args>
	void Log(LogLevel level, const char* file, int32 line,
	         std::format_string<Args...> fmt, Args&&... args)
	{
		String message = std::format(fmt, std::forward<Args>(args)...);
		Log(level, file, line, message);
	}

	void EnableFileLogging(bool enabled)    { m_fileEnabled = enabled; }
	void EnableConsoleLogging(bool enabled) { m_consoleEnabled = enabled; }

private:
	Logger() = default;
	~Logger();

	void ConsoleWrite(LogLevel level, const String& formatted);
	void FileWriterThread();
	String FormatTimestamp() const;
	static const char* StripPath(const char* path);

	bool m_consoleEnabled = true;
	bool m_fileEnabled    = false;
	bool m_initialized    = false;

	// Async file writer.
	// BufferedContainer handles data synchronization via Futex.
	// The CV is used only to wake the writer thread — not for data protection.
	BufferedContainer<String, Vector<String>> m_fileBuffer;
	URef<std::thread>       m_writerThread;
	std::mutex              m_writerWakeMutex;  // Paired with CV for wake signaling only.
	std::condition_variable m_writerWakeCV;
	std::atomic<bool>       m_shutdownWriter{false};
	std::ofstream           m_logFile;

	static constexpr const char* k_levelStrings[] = { "DEBUG", "INFO", "WARNING", "ERROR" };
	static constexpr const char* k_levelColors[]  = { "\033[0m", "\033[35m", "\033[33m", "\033[31m" };
};

// ---------------------------------------------------------------------------
// Macros
// ---------------------------------------------------------------------------

#define LOG_ERROR(fmt, ...) \
	Logger::Get().Log(LogLevel::Error, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)

#if LOG_WARN_ENABLED
#define LOG_WARNING(fmt, ...) \
	Logger::Get().Log(LogLevel::Warning, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)
#else
#define LOG_WARNING(fmt, ...) do {} while(0)
#endif

#if LOG_INFO_ENABLED
#define LOG_INFO(fmt, ...) \
	Logger::Get().Log(LogLevel::Info, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)
#else
#define LOG_INFO(fmt, ...) do {} while(0)
#endif

#if LOG_DEBUG_ENABLED
#define LOG_DEBUG(fmt, ...) \
	Logger::Get().Log(LogLevel::Debug, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...) do {} while(0)
#endif
