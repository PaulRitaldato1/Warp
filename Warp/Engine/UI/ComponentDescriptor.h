#pragma once

#include <Common/CommonTypes.h>
#include <Core/ECS/World.h>

// Describes a component type at runtime: its name, size, and how to draw its
// ImGui inspector and add/remove it from an entity.
struct ComponentDescriptor
{
	u32         id;
	const char* name;
	size_t      size;
	void (*drawUI)(void* componentPtr);
	void (*addToEntity)(World& world, Entity entity);
	void (*removeFromEntity)(World& world, Entity entity);
};

// Single global registry of component descriptors, exported from the DLL.
WARP_API HashMap<u32, ComponentDescriptor>& GetComponentDescriptors();

// Template specialization point — each component that wants editor UI
// specializes this struct and provides a static Draw() function plus a
// static inline bool registered member that triggers self-registration.
template <typename T>
struct ComponentUI; // intentionally undefined — specialization required

// Call once per component type (typically via static inline bool in the
// ComponentUI specialization). Registers the descriptor so the generic
// inspector can discover and draw it.
template <typename T>
bool RegisterComponentUI(const char* name,
                         void (*drawFunction)(T&),
                         void (*removeFunction)(World&, Entity) = nullptr)
{
	ComponentDescriptor descriptor;
	descriptor.id               = ComponentID<T>::Get();
	descriptor.name             = name;
	descriptor.size             = sizeof(T);
	descriptor.drawUI           = [](void* ptr) { ComponentUI<T>::Draw(*static_cast<T*>(ptr)); };
	descriptor.addToEntity      = [](World& world, Entity entity) { world.AddComponent<T>(entity); };
	descriptor.removeFromEntity = removeFunction
	    ? removeFunction
	    : [](World& world, Entity entity) { world.RemoveComponent<T>(entity); };

	GetComponentDescriptors()[descriptor.id] = descriptor;
	return true;
}
