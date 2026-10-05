#pragma once

#include <Common/CommonTypes.h>
#include <Core/Console/ConsoleRegistry.h>

// The words typed after a name, split on spaces. Quotes group words: say "hello there"
using ConsoleArgs = Vector<String>;

// Anything the console can reach by name: variables and commands. The registry
// and the console only see this, so they never need to know which one it is.
class ConsoleObject
{
public:
	// Registers on construction and unregisters on destruction. Console objects
	// are globals, so this runs before main and after it returns.
	ConsoleObject(const char* name, const char* help)
		: m_name(name)
		, m_help(help)
	{
		ConsoleRegistry::Get().Register(this);
	}

	// Safe at exit: the registry is created by the first registration, so as a
	// static it is destroyed after every object constructed after it.
	virtual ~ConsoleObject()
	{
		ConsoleRegistry::Get().Unregister(this);
	}

	ConsoleObject(const ConsoleObject&)			   = delete;
	ConsoleObject& operator=(const ConsoleObject&) = delete;

	// What typing the name runs. Logs its own output and errors. False on bad
	// input, so a config file can report the line.
	virtual bool Execute(const ConsoleArgs& args) = 0;

	const String& GetName() const
	{
		return m_name;
	}

	const String& GetHelp() const
	{
		return m_help;
	}

private:
	String m_name;
	String m_help;
};
