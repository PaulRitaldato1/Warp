#pragma once

#include <Common/CommonTypes.h>
#include <Core/ECS/Entity.h>

// Per-component-type metadata needed by Archetype for type-erased storage.
// Since IsComponent enforces trivially copyable + trivially destructible,
// no destroy/moveConstruct function pointers needed — memcpy suffices.
struct ComponentInfo
{
	u32    id;
	size_t size;
	size_t alignment;
};

namespace detail
{
	inline u32& ComponentCounter()
	{
		static u32 counter = 0;
		return counter;
	}

	inline Vector<ComponentInfo>& ComponentRegistry()
	{
		static Vector<ComponentInfo> registry;
		return registry;
	}
}

// Registration happens automatically when ComponentID<T>::Get() is first called.
// This ensures the registry is populated before any archetype needs it.
template <typename T>
struct ComponentID
{
	static u32 Get()
	{
		static u32 id = []() {
			u32 newId = detail::ComponentCounter()++;

			ComponentInfo componentInfo;
			componentInfo.id        = newId;
			componentInfo.size      = sizeof(T);
			componentInfo.alignment = alignof(T);

			Vector<ComponentInfo>& registry = detail::ComponentRegistry();
			if (registry.size() <= newId)
			{
				registry.resize(newId + 1);
			}
			registry[newId] = componentInfo;

			return newId;
		}();
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
