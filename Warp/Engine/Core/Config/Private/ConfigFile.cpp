#include <Core/Config/ConfigFile.h>

#include <Debugging/Logging.h>

#include <fstream>

static String Trim(const String& text)
{
	const size_t first = text.find_first_not_of(" \t\r");
	if (first == String::npos)
	{
		return {};
	}
	const size_t last = text.find_last_not_of(" \t\r");
	return text.substr(first, last - first + 1);
}

ConfigFile ConfigFile::Load(const String& path)
{
	ConfigFile config;

	std::ifstream file(path);
	if (!file)
	{
		LOG_WARNING("ConfigFile: {} not found, using defaults", path);
		return config;
	}

	String line;
	u32 lineNumber = 0;
	while (std::getline(file, line))
	{
		++lineNumber;

		// Comments can also trail a value: r.Shadows = 1 # on
		const size_t comment = line.find('#');
		if (comment != String::npos)
		{
			line.erase(comment);
		}

		line = Trim(line);
		if (line.empty())
		{
			continue;
		}

		const size_t equals = line.find('=');
		if (equals == String::npos)
		{
			LOG_WARNING("ConfigFile: {}:{} has no '=', skipped", path, lineNumber);
			continue;
		}

		const String key = Trim(line.substr(0, equals));
		if (key.empty())
		{
			LOG_WARNING("ConfigFile: {}:{} has no key, skipped", path, lineNumber);
			continue;
		}

		config.m_values[key] = Trim(line.substr(equals + 1));
	}

	LOG_DEBUG("ConfigFile: loaded {} ({} values)", path, config.m_values.size());
	return config;
}

const ConfigFile& ConfigFile::GetEngine()
{
	// Baked in by CMake so the file is read from the repo, not the build folder.
	static const ConfigFile engine = Load(String(WARP_CONFIG_DIR) + "/Engine.ini");
	return engine;
}

std::optional<String> ConfigFile::Get(const String& key) const
{
	auto it = m_values.find(key);
	if (it == m_values.end())
	{
		return std::nullopt;
	}
	return it->second;
}
