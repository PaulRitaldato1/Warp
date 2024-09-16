#pragma once


#ifndef NDEBUG
#include <iostream>

#define DYNAMIC_ASSERT(condition, message) \
    do { \
        if(!(condition)){\
            std::cerr << "\033[1;33mDynamic Assert failure: `" #condition "` Failed in " << __FILE__  \
                      << " line " << __LINE__ << " with message: " << message << ".\033[0m\n";\
        }\
    }while(false)

#define FATAL_ASSERT(condition, message) \
    do{ \
        if(!(condition)){\
            std::cerr << "\033[1;31mFATAL ASSERT FAILURE: `" #condition "` Failed in " << __FILE__ \
                      << " line " << __LINE__ << " with message: " << message << ".\033[0m\n";\
            std::terminate();\
        }\
    }while(false)
#else
#define DYNAMIC_ASSERT(condition, message) do {} while (false)
#define FATAL_ASSERT(condition, message) do {} while (false)
#endif