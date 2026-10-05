#pragma once

#include <Common/CommonTypes.h>

// A key = value file of console variables. # starts a comment, blank lines are
// ignored, and whitespace around keys and values is trimmed.
class WARP_API ConfigFile
{
public:
	// A missing or unreadable file gives an empty config, so every cvar keeps its default.
	static ConfigFile Load(const String& path);

	// The engine's Config/Engine.ini, from the repo.
	static ConfigFile LoadEngine();

	// Sets each value through the console registry, in file order, exactly as if
	// typed into the console. Unknown names and bad values warn with the line.
	void Apply() const;

private:
	struct Entry
	{
		String key;
		String value;
		u32 lineNumber;
	};

	String m_path;
	Vector<Entry> m_entries;
};
