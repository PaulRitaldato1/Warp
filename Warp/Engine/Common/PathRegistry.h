#pragma once

#include <Common/CommonTypes.h>

#include <mutex>

// Interns asset paths so components can hold a 4 byte id instead of a fixed size
// character buffer. Ids are stable and interning is permanent for the process.
//
// Strings are stored indirectly so that Resolve() references stay valid when a
// later Intern() grows the table.
class WARP_API PathRegistry
{
public:
	static constexpr u32 k_invalidId = ~0u;

	// Returns the id for this path, adding it if unseen. Empty paths give k_invalidId.
	u32 Intern(const String& path);

	// Returns an empty string for k_invalidId or any id that was never issued.
	const String& Resolve(u32 id) const;

	u32 Count() const;

private:
	Vector<URef<String>> m_paths;
	HashMap<String, u32> m_lookup;
	mutable std::mutex	 m_mutex;
};

WARP_API PathRegistry& GetPathRegistry();
