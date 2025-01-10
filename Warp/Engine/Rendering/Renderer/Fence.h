#pragma once

#include <Common/CommonTypes.h>

class Fence
{
public:
	virtual ~Fence() = default;

	virtual void Initialize() = 0;

	virtual void Wait(u64 Timeout = UINT64_MAX) = 0;

	virtual void Reset() = 0;

	virtual void Signal() = 0;

private:
};