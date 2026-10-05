#pragma once

#include <Common/CommonTypes.h>

class ConsoleVariableBase;

// Every console variable, found by name. Names are case insensitive: lookups
// lowercase the key, and each variable keeps its own spelling for display.
class WARP_API ConsoleRegistry
{
public:
	// Function-local static, so it exists before the first Cvar registers, even
	// though Cvars are globals constructed before main in unspecified order.
	// Exported so the engine and the game share one registry.
	static ConsoleRegistry& Get();

	void Register(ConsoleVariableBase* variable);
	void Unregister(ConsoleVariableBase* variable);

	// nullptr if no variable has that name.
	ConsoleVariableBase* Find(const String& name) const;

private:
	HashMap<String, ConsoleVariableBase*> m_variables; // keyed by lowercased name
};
