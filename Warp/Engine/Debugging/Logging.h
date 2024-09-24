#pragma once

#include <Common/CommonTypes.h>
#include <iostream>

enum LOG_LEVEL
{
    LOG_INFO = 0,
    LOG_DEBUG,
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

    inline static const Array<String, LOG_LEVEL_COUNT> logLevelStrings = { "LOG_INFO", "LOG_DEBUG", "LOG_WARNING", "LOG_ERROR"};
    inline static const Array<String, LOG_LEVEL_COUNT> logLevelColors = {"\033[0m", "\033[35m", "\033[33m", "\033[31m"};

    //TODO: File bullshit here 
};

namespace Logging
{
    WARP_API inline void Log(String& outStr, LOG_LEVEL level)
    {
        Logger::Get().ConsoleLog(outStr, level);
    }

    WARP_API inline void Log(String&& outStr, LOG_LEVEL level)
    {
        Logger::Get().ConsoleLog(outStr, level);
    }

    WARP_API inline void LogError(String& outStr)
    {
        Logger::Get().ConsoleLog(outStr, LOG_LEVEL::LOG_ERROR);
    }

    WARP_API inline void LogError(String&& outStr)
    {
        Logger::Get().ConsoleLog(outStr, LOG_LEVEL::LOG_ERROR);
    }
    
    WARP_API inline void LogWarning(String& outStr)
    {
        Logger::Get().ConsoleLog(outStr, LOG_LEVEL::LOG_WARNING);
    }

    WARP_API inline void LogWarning(String&& outStr)
    {
        Logger::Get().ConsoleLog(outStr, LOG_LEVEL::LOG_WARNING);
    }

    WARP_API inline void LogDebug(String& outStr)
    {
        Logger::Get().ConsoleLog(outStr, LOG_LEVEL::LOG_DEBUG);
    }

    WARP_API inline void LogDebug(String&& outStr)
    {
        Logger::Get().ConsoleLog(outStr, LOG_LEVEL::LOG_DEBUG);
    }

    WARP_API inline void LogInfo(String& outStr)
    {
        Logger::Get().ConsoleLog(outStr, LOG_LEVEL::LOG_INFO);
    }

    WARP_API inline void LogInfo(String&& outStr)
    {
        Logger::Get().ConsoleLog(outStr, LOG_LEVEL::LOG_INFO);
    }
}