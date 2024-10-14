#pragma once

#include <Common/CommonTypes.h>
#include <iostream>

#define LOG_WARN_ENABLED 1
#define LOG_INFO_ENABLED 1
#define LOG_DEBUG_ENABLED 0

#ifdef WARP_RELEASE
#define LOG_DEBUG_ENABLED 0
#endif

enum LOG_LEVEL
{
    LOG_DEBUG = 0,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,

    LOG_LEVEL_COUNT
};


class Logger
{

public:

    inline void ConsoleLog(const String& outStr, LOG_LEVEL level)
    {
        ConsoleLogInternal(outStr, level);
    }

    inline void ConsoleLog(const String&& outStr, LOG_LEVEL level)
    {
        ConsoleLogInternal(static_cast<const String&>(outStr), level);
    }

    inline void FileLog(const String& outStr, LOG_LEVEL level)
    {
        ConsoleLogInternal(outStr, level);
    }

    inline void FileLog(const String&& outStr, LOG_LEVEL level)
    {
        ConsoleLogInternal(static_cast<const String&>(outStr), level);
    }

    //Both File and Console log
    inline void Log(const String& outStr, LOG_LEVEL level)
    {
        ConsoleLogInternal(outStr, level);
    }

    inline void Log(const String&& outStr, LOG_LEVEL level)
    {
        ConsoleLogInternal(static_cast<const String&>(outStr), level);
    }

    static Logger& Get()
    {
        static Logger Instance;
        return Instance;
    }

private:

    inline void ConsoleLogInternal(const String& outStr, LOG_LEVEL level)
    {
        std::clog << logLevelColors[level] << logLevelStrings[level] << ": " << outStr << "\033[0m\n";
    }

    const Array<String, LOG_LEVEL_COUNT> logLevelStrings = { "LOG_DEBUG", "LOG_INFO", "LOG_WARNING", "LOG_ERROR"};
    const Array<String, LOG_LEVEL_COUNT> logLevelColors = {"\033[0m", "\033[35m", "\033[33m", "\033[31m"};

    //TODO: File bullshit here 
};

//Error Log
#define LOG_ERROR(message) Logger::Get().ConsoleLog(message, LOG_LEVEL::LOG_ERROR);

//warning log
#if LOG_WARN_ENABLED > 0 
#define LOG_WARNING(message) Logger::Get().ConsoleLog(message, LOG_LEVEL::LOG_WARNING);
#else
#define LOG_WARNING(message) do { } while(0);
#endif

//info log
#if LOG_INFO_ENABLED > 0 
#define LOG_INFO(message) Logger::Get().ConsoleLog(message, LOG_LEVEL::LOG_INFO);
#else
#define LOG_INFO(message) do { } while(0);
#endif

//debug log
#if LOG_DEBUG_ENABLED > 0
#define LOG_DEBUG(message) Logger::Get().ConsoleLog(message, LOG_LEVEL::LOG_DEBUG); 
#else
#define LOG_DEBUG(message) do { } while(0);
#endif