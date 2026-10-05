#pragma once

#include <Common/CommonTypes.h>

class ConsoleObject;

// Every console variable and command, found by name. Names are case insensitive:
// lookups lowercase the key, and each object keeps its own spelling for display.
class WARP_API ConsoleRegistry
{
public:
	// Function-local static, so it exists before the first object registers, even
	// though they are globals constructed before main in unspecified order.
	// Exported so the engine and the game share one registry.
	static ConsoleRegistry& Get();

	void Register(ConsoleObject* object);
	void Unregister(ConsoleObject* object);

	// nullptr if nothing has that name.
	ConsoleObject* Find(const String& name) const;

	// Every object, sorted by name, for listing and completion.
	Vector<ConsoleObject*> GetAll() const;

	// Runs one typed line, like "r.GPUCulling 0". What the console and config
	// files call. False if the name is unknown or the arguments were rejected.
	bool Execute(const String& line);

	// Called once the values only read at startup have been read, like
	// r.GraphicsAPI. The console refuses to change one after this.
	void FinishStartup()
	{
		m_bStartupFinished = true;
	}

	bool IsStartupFinished() const
	{
		return m_bStartupFinished;
	}

private:
	HashMap<String, ConsoleObject*> m_objects; // keyed by lowercased name
	bool m_bStartupFinished = false;
};
