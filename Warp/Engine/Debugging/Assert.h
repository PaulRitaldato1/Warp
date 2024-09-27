#pragma once

#define WARP_ASSERTIONS_ENABLED

#ifdef _WIN32
#include <intrin.h>
#define DebugBreak() __debugbreak()
#else
#define DebugBreak() __builtin_trap()
#endif

#ifdef WARP_ASSERTIONS_ENABLED
#include <iostream>

#define DYNAMIC_ASSERT(condition, message) \
    if(condition) {} \
    else \
    { \
     std::cerr << "\033[1;33mDynamic Assert failure: `" #condition "` Failed in " << __FILE__  \
                      << " line " << __LINE__ << " with message: " << message << ".\033[0m\n"; \
        DebugBreak(); \
    }

#define FATAL_ASSERT(condition, message) \
    if(condition){} \
    else \
    { \
        std::cerr << "\033[1;31mFATAL ASSERT FAILURE: `" #condition "` Failed in " << __FILE__ \
            << " line " << __LINE__ << " with message: " << message << ".\033[0m\n";\
        std::terminate();\
    }
#else
#define DYNAMIC_ASSERT(condition, message) do {} while (false)
#define FATAL_ASSERT(condition, message) do {} while (false)
#endif