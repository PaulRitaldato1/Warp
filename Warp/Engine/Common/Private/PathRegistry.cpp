#include <Common/PathRegistry.h>

u32 PathRegistry::Intern(const String& path)
{
	if (path.empty())
	{
		return k_invalidId;
	}

	std::scoped_lock<std::mutex> lock(m_mutex);

	auto it = m_lookup.find(path);
	if (it != m_lookup.end())
	{
		return it->second;
	}

	u32 id = static_cast<u32>(m_paths.size());
	m_paths.push_back(std::make_unique<String>(path));
	m_lookup[path] = id;
	return id;
}

const String& PathRegistry::Resolve(u32 id) const
{
	static const String empty;

	std::scoped_lock<std::mutex> lock(m_mutex);

	if (id >= m_paths.size())
	{
		return empty;
	}
	return *m_paths[id];
}

u32 PathRegistry::Count() const
{
	std::scoped_lock<std::mutex> lock(m_mutex);
	return static_cast<u32>(m_paths.size());
}

PathRegistry& GetPathRegistry()
{
	static PathRegistry instance;
	return instance;
}
