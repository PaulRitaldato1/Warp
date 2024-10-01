#pragma once

#include <Common/CommonTypes.h>

enum AllocationType
{
    ALLOC_TYPE_UNKNOWN,
    ALLOC_TYPE_ARRAY,
    ALLOC_TYPE_STRING,
    ALLOC_TYPE_JOB,
    ALLOC_TYPE_MAP,
    ALLOC_TYPE_APPLICATION,
    ALLOC_TYPE_TEXTURE,
    ALLOC_TYPE_MATERIAL,
    ALLOC_TYPE_RENDERER,
    ALLOC_TYPE_GAME,
    ALLOC_TYPE_TRANSFORM,
    ALLOC_TYPE_ENTITY,
    ALLOC_TYPE_COMPONENT,
    ALLOC_TYPE_SYSTEM,
    ALLOC_TYPE_SCENE,

    ALLOC_TYPE_SIZE
};

class AllocationManagerBase
{
public:
   AllocationManagerBase() : m_totalAllocated(0) 
   {}

    virtual ~AllocationManagerBase() {}

    virtual void* WarpAllocate(u64 size, AllocationType type);
    virtual void WarpFree(void* block, AllocationType type);
    virtual void WarpZeroMemory(void* block, u64 size);
    virtual void WarpCopyMemory(void* dest, const void* source, u64 size);
    virtual void WarpSetMemory(void* dest, int32 value, u64 size);

private:

    //total size in bytes
    int64 m_totalAllocated;
};