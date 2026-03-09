#pragma once

#include <Common/CommonTypes.h>
#include <Core/ECS/Entity.h>
#include <typeindex>

// Per-component-type metadata needed by Archetype for type-erased storage.
// Since IsComponent enforces trivially copyable + trivially destructible,
// no destroy/moveConstruct function pointers needed — memcpy suffices.
struct ComponentInfo
{
	u32    id;
	size_t size;
	size_t alignment;
};

// Exported from WarpEngine.dll — single instance across all modules.
// Returns the global component registry used by all archetypes.
WARP_API Vector<ComponentInfo>& GetComponentRegistry();

// Registers a component type by std::type_index and returns its stable ID.
// Idempotent: calling multiple times (from EXE and DLL) always returns the same ID.
// This avoids the DLL-boundary static duplication problem with inline functions.
WARP_API u32 RegisterOrGetComponentId(std::type_index typeIndex, size_t size, size_t alignment);

// Registration happens automatically when ComponentID<T>::Get() is first called.
// Uses RegisterOrGetComponentId so the ID is consistent across the DLL boundary.
template <typename T>
struct ComponentID
{
	static u32 Get()
	{
		static u32 id = RegisterOrGetComponentId(
			std::type_index(typeid(T)), sizeof(T), alignof(T));
		return id;
	}
};

// C++20 concept — enforced at compile time on all ECS operations.
// Components must be plain data: trivially copyable and trivially destructible.
template <typename T>
concept IsComponent = std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T>;

// Helper: build a ComponentMask from a parameter pack of component types.
template <IsComponent... Ts>
ComponentMask BuildMask()
{
	ComponentMask mask;
	(mask.set(ComponentID<Ts>::Get()), ...);
	return mask;
}

// Hash for ComponentMask — std::bitset has no default hash.
struct ComponentMaskHash
{
	size_t operator()(const ComponentMask& mask) const
	{
		return std::hash<u64>{}(mask.to_ullong());
	}
};
