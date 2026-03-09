#include <Core/ECS/Component.h>
#include <unordered_map>

Vector<ComponentInfo>& GetComponentRegistry()
{
	static Vector<ComponentInfo> registry;
	return registry;
}

u32 RegisterOrGetComponentId(std::type_index typeIndex, size_t size, size_t alignment)
{
	static std::unordered_map<std::type_index, u32> typeToId;
	static u32 counter = 0;

	auto it = typeToId.find(typeIndex);
	if (it != typeToId.end())
	{
		return it->second;
	}

	u32 newId = counter++;

	ComponentInfo info;
	info.id        = newId;
	info.size      = size;
	info.alignment = alignment;

	Vector<ComponentInfo>& registry = GetComponentRegistry();
	if (registry.size() <= newId)
	{
		registry.resize(newId + 1);
	}
	registry[newId] = info;

	typeToId[typeIndex] = newId;
	return newId;
}
