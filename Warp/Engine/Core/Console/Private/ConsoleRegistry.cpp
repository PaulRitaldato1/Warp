#include <Core/Console/ConsoleRegistry.h>

#include <Core/Console/ConsoleCommand.h>
#include <Core/Console/ConsoleObject.h>
#include <Debugging/Assert.h>
#include <Debugging/Logging.h>

#include <algorithm>
#include <cctype>

static String ToLower(const String& text)
{
	String lower = text;
	std::transform(lower.begin(), lower.end(), lower.begin(),
				   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return lower;
}

// Splits on whitespace. Quotes group words and are dropped: say "a  b" gives
// say and a  b, spacing kept.
static Vector<String> Tokenize(const String& line)
{
	Vector<String> tokens;
	String current;
	bool bInQuotes = false;
	bool bInToken  = false;

	for (const char c : line)
	{
		if (c == '"')
		{
			bInQuotes = !bInQuotes;
			bInToken  = true; // so "" still counts as an empty argument
		}
		else if (!bInQuotes && std::isspace(static_cast<unsigned char>(c)))
		{
			if (bInToken)
			{
				tokens.push_back(std::move(current));
				current.clear();
				bInToken = false;
			}
		}
		else
		{
			current += c;
			bInToken = true;
		}
	}

	if (bInToken)
	{
		tokens.push_back(std::move(current));
	}

	return tokens;
}

ConsoleRegistry& ConsoleRegistry::Get()
{
	static ConsoleRegistry registry;
	return registry;
}

void ConsoleRegistry::Register(ConsoleObject* object)
{
	const String key = ToLower(object->GetName());

	// Two objects sharing a name is always a bug, and the console could only
	// ever reach one of them.
	FATAL_ASSERT(m_objects.find(key) == m_objects.end(),
				 "ConsoleRegistry: a console variable or command with this name exists");

	m_objects[key] = object;
}

void ConsoleRegistry::Unregister(ConsoleObject* object)
{
	m_objects.erase(ToLower(object->GetName()));
}

ConsoleObject* ConsoleRegistry::Find(const String& name) const
{
	auto it = m_objects.find(ToLower(name));
	return it != m_objects.end() ? it->second : nullptr;
}

Vector<ConsoleObject*> ConsoleRegistry::GetAll() const
{
	Vector<ConsoleObject*> objects;
	objects.reserve(m_objects.size());
	for (const auto& [key, object] : m_objects)
	{
		objects.push_back(object);
	}

	std::sort(objects.begin(), objects.end(), [](const ConsoleObject* a, const ConsoleObject* b)
			  { return ToLower(a->GetName()) < ToLower(b->GetName()); });
	return objects;
}

bool ConsoleRegistry::Execute(const String& line)
{
	Vector<String> tokens = Tokenize(line);
	if (tokens.empty())
	{
		return true;
	}

	ConsoleObject* object = Find(tokens[0]);
	if (object == nullptr)
	{
		LOG_WARNING("Unknown command or variable '{}'", tokens[0]);
		return false;
	}

	const ConsoleArgs args(tokens.begin() + 1, tokens.end());
	return object->Execute(args);
}

// "help" lists everything, "help r." lists names starting with r.
static ConsoleCommand CmdHelp("help", "List commands and variables, optionally only those starting with a prefix",
							  [](const ConsoleArgs& args)
							  {
								  const String prefix = args.empty() ? String() : ToLower(args[0]);
								  for (const ConsoleObject* object : ConsoleRegistry::Get().GetAll())
								  {
									  if (ToLower(object->GetName()).starts_with(prefix))
									  {
										  LOG_INFO("{}  {}", object->GetName(), object->GetHelp());
									  }
								  }
							  });
