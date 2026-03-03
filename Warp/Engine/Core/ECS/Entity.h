#pragma once

#include <Common/CommonTypes.h>
#include <bitset>

using ComponentMask = std::bitset<64>;

struct Entity
{
	u32 id         = 0;
	u32 generation = 0;
};

inline bool operator==(Entity a, Entity b) { return a.id == b.id && a.generation == b.generation; }
inline bool operator!=(Entity a, Entity b) { return !(a == b); }

static constexpr Entity k_nullEntity = {0, 0};
