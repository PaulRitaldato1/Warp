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

	// Executes one or more command lists as a single batch and signals the
	// internal fence once.  Returns the fence value that was signaled —
	// store this in the corresponding FrameSyncPoint to know when the GPU
	// is done.  Batching avoids per-submit kernel overhead.
	virtual u64  Submit(const Vector<CommandList*>& lists) = 0;

	// Convenience: submit a single command list.
	u64 Submit(CommandList& list)
	{
		CommandList* ptr = &list;
		return Submit(Vector<CommandList*>{ptr});
	}

	// Block the CPU until the GPU reaches the given fence value.
	virtual void WaitForValue(u64 value)   = 0;

	// Returns the latest fence value the GPU has completed (non-blocking).
	virtual u64  GetCompletedValue() const = 0;

	// GPU-side wait: this queue will not start executing subsequent work
	// until the other queue's fence reaches the given value.
	// Does NOT stall the CPU — only inserts a GPU dependency.
	virtual void WaitForQueue(CommandQueue& other, u64 fenceValue) = 0;

	virtual void Reset()                   = 0;
};
