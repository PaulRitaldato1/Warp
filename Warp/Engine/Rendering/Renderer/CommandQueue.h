#pragma once

class ICommandList;

class ICommandQueue
{
public:
	ICommandQueue()	 = default;
	~ICommandQueue() = default;

	virtual void Begin()					= 0;
	virtual void End()						= 0;
	virtual void Submit(ICommandList& List) = 0;
	virtual void Reset()					= 0;
};