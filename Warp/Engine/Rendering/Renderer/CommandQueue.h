#pragma once

class CommandList;

class CommandQueue
{
public:
	CommandQueue()	= default;
	~CommandQueue() = default;

	virtual void Begin()				   = 0;
	virtual void End()					   = 0;
	virtual void Submit(CommandList& List) = 0;
	virtual void Reset()				   = 0;
};