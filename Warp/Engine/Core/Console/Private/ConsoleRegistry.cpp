#include <Core/Console/ConsoleRegistry.h>

#include <Core/Console/ConsoleVariable.h>
#include <Debugging/Assert.h>

#include <algorithm>
#include <cctype>

static String ToLower(const String& text)
{
	String lower = text;
	std::transform(lower.begin(), lower.end(), lower.begin(),
				   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return lower;
}

ConsoleRegistry& ConsoleRegistry::Get()
{
	static ConsoleRegistry registry;
	return registry;
}

void ConsoleRegistry::Register(ConsoleVariableBase* variable)
{
	const String key = ToLower(variable->GetName());

	// Two variables sharing a name is always a bug, and the console could only
	// ever reach one of them.
	FATAL_ASSERT(m_variables.find(key) == m_variables.end(), "ConsoleRegistry: a console variable with this name exists");

	m_variables[key] = variable;
}

void ConsoleRegistry::Unregister(ConsoleVariableBase* variable)
{
	m_variables.erase(ToLower(variable->GetName()));
}

ConsoleVariableBase* ConsoleRegistry::Find(const String& name) const
{
	auto it = m_variables.find(ToLower(name));
	return it != m_variables.end() ? it->second : nullptr;
}
