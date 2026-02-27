#pragma once

#include <Common/CommonTypes.h>

class CommandList;

enum class CommandQueueType
{
	Graphics,
	Compute,
	Copy
};

class CommandQueue
{
public:
	virtual ~CommandQueue() = default;

	// Executes the command list and signals the internal fence.
	// Returns the fence value that was signaled — store this in the
	// corresponding FrameSyncPoint to know when the GPU is done.
	virtual u64  Submit(CommandList& list) = 0;

	// Block the CPU until the GPU reaches the given fence value.
	virtual void WaitForValue(u64 value)   = 0;

	virtual void Reset()                   = 0;
};
