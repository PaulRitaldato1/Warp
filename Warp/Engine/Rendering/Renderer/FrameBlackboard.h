#pragma once

#include <Common/CommonTypes.h>
#include <Debugging/Assert.h>

#include <any>
#include <typeindex>

// Where passes hand their outputs to later passes, keyed by type:
//   blackboard.Add(GBufferOutput{ ... });            // GBufferPass
//   blackboard.Get<GBufferOutput>().albedo;          // LightingPass
// One per frame. Execute lambdas copy what they need out of it, the same as they
// copy the rest of their inputs, so none of them hold references into it.
class FrameBlackboard
{
public:
	template <typename T>
	void Add(T value)
	{
		const auto [it, bInserted] = m_entries.try_emplace(std::type_index(typeid(T)), std::move(value));
		DYNAMIC_ASSERT(bInserted, "FrameBlackboard::Add: this output was already added this frame");
	}

	// Asserts when nothing added it, i.e. the producing pass runs later or not at all.
	template <typename T>
	const T& Get() const
	{
		const T* value = TryGet<T>();
		FATAL_ASSERT(value, "FrameBlackboard::Get: nothing added this output, check the pass order in DrawDeferred");
		return *value;
	}

	// Null when nothing added it, for passes whose input is optional.
	template <typename T>
	const T* TryGet() const
	{
		const auto it = m_entries.find(std::type_index(typeid(T)));
		return it != m_entries.end() ? std::any_cast<T>(&it->second) : nullptr;
	}

private:
	HashMap<std::type_index, std::any> m_entries;
};
