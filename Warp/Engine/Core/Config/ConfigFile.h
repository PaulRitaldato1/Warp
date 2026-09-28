#pragma once

#include <Common/CommonTypes.h>

#include <optional>

// A key = value file. # starts a comment, blank lines are ignored, and
// whitespace around keys and values is trimmed.
class WARP_API ConfigFile
{
public:
	// Missing or unreadable files give an empty config, so every key falls back
	// to its default.
	static ConfigFile Load(const String& path);

	// The engine's Config/Engine.ini, from the repo.
	static const ConfigFile& GetEngine();

	std::optional<String> Get(const String& key) const;

private:
	HashMap<String, String> m_values;
};
