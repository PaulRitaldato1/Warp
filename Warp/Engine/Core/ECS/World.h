#pragma once

#include <Core/ECS/Archetype.h>
#include <Core/ECS/System.h>
#include <Debugging/Assert.h>

class WARP_API World
{
public:
	World()                            = default;
	World(const World&)                = delete;
	World& operator=(const World&)     = delete;
	World(World&&) noexcept            = default;
	World& operator=(World&&) noexcept = default;

	// Entity lifecycle
	Entity CreateEntity();
	void   DestroyEntity(Entity entity);
	bool   IsAlive(Entity entity) const;

	// Component operations
	template <IsComponent T>
	T& AddComponent(Entity entity, const T& component = T{})
	{
		FATAL_ASSERT(IsAlive(entity), "World::AddComponent: entity is not alive");
		FATAL_ASSERT(!HasComponent<T>(entity), "World::AddComponent: entity already has this component");

		EntityRecord& record = m_entities[entity.id];

		// Build the new mask with this component added.
		ComponentMask newMask;
		if (record.archetype)
		{
			newMask = record.archetype->GetMask();
		}
		newMask.set(ComponentID<T>::Get());

		Archetype* newArchetype = FindOrCreateArchetype(newMask);

		// Migrate entity from old archetype to new one.
		if (record.archetype)
		{
			MoveEntity(entity, record.archetype, record.row, newArchetype);
		}
		else
		{
			u32 newRow = newArchetype->AddEntity(entity);
			record.archetype = newArchetype;
			record.row = newRow;
		}

		// Write the component data into the new archetype.
		T* column = newArchetype->GetColumn<T>();
		column[record.row] = component;

		return column[record.row];
	}

	template <IsComponent T>
	void RemoveComponent(Entity entity)
	{
		FATAL_ASSERT(IsAlive(entity), "World::RemoveComponent: entity is not alive");
		FATAL_ASSERT(HasComponent<T>(entity), "World::RemoveComponent: entity does not have this component");

		EntityRecord& record = m_entities[entity.id];

		// Build the new mask with this component removed.
		ComponentMask newMask = record.archetype->GetMask();
		newMask.reset(ComponentID<T>::Get());

		if (newMask.none())
		{
			// No components left — remove from archetype, entity becomes componentless.
			Entity swappedEntity = record.archetype->RemoveEntity(record.row);
			if (swappedEntity != k_nullEntity)
			{
				FixupSwappedEntity(swappedEntity, record.row);
			}
			record.archetype = nullptr;
			record.row = 0;
		}
		else
		{
			Archetype* newArchetype = FindOrCreateArchetype(newMask);
			MoveEntity(entity, record.archetype, record.row, newArchetype);
		}
	}

	template <IsComponent T>
	T& GetComponent(Entity entity)
	{
		FATAL_ASSERT(HasComponent<T>(entity), "World::GetComponent: entity does not have this component");

		EntityRecord& record = m_entities[entity.id];
		T* column = record.archetype->GetColumn<T>();
		return column[record.row];
	}

	template <IsComponent T>
	const T& GetComponent(Entity entity) const
	{
		FATAL_ASSERT(HasComponent<T>(entity), "World::GetComponent: entity does not have this component");

		const EntityRecord& record = m_entities[entity.id];
		const T* column = record.archetype->GetColumn<T>();
		return column[record.row];
	}

	template <IsComponent T>
	bool HasComponent(Entity entity) const
	{
		if (!IsAlive(entity))
		{
			return false;
		}

		const EntityRecord& record = m_entities[entity.id];
		if (!record.archetype)
		{
			return false;
		}

		return record.archetype->GetMask().test(ComponentID<T>::Get());
	}

	// Query — calls fn(Entity, T1&, T2&, ...) for every entity with all of <Ts...>.
	template <IsComponent... Ts, typename Func>
	void Each(Func&& fn)
	{
		ComponentMask queryMask = BuildMask<Ts...>();

		for (auto& [mask, archetype] : m_archetypes)
		{
			// Check if this archetype has all queried components (superset check).
			if ((mask & queryMask) != queryMask)
			{
				continue;
			}

			u32 entityCount = archetype->GetEntityCount();
			if (entityCount == 0)
			{
				continue;
			}

			const Vector<Entity>& entities = archetype->GetEntities();

			// Get typed column pointers for each queried component.
			auto callPerEntity = [&](auto*... columns)
			{
				for (u32 row = 0; row < entityCount; ++row)
				{
					fn(entities[row], columns[row]...);
				}
			};

			callPerEntity(archetype->GetColumn<Ts>()...);
		}
	}

	// --- System management ---

	// Register a system. Calls Init() immediately. Systems run in registration order.
	template <IsSystem T, typename... Args>
	T& RegisterSystem(Args&&... args)
	{
		URef<System> system = std::make_unique<T>(std::forward<Args>(args)...);
		T& ref = static_cast<T&>(*system);
		system->Init(*this);
		m_systems.push_back(std::move(system));
		return ref;
	}

	// Update all registered systems in registration order.
	void UpdateSystems(f32 deltaTime);

	// Shutdown and remove all systems.
	void ShutdownSystems();

private:
	struct EntityRecord
	{
		Archetype* archetype  = nullptr;
		u32        row        = 0;
		u32        generation = 0;
		bool       alive      = false;
	};

	Archetype* FindOrCreateArchetype(ComponentMask mask);
	void       MoveEntity(Entity entity, Archetype* from, u32 fromRow, Archetype* to);

	// Fixes up the entity record for an entity that was swap-moved within an archetype.
	void       FixupSwappedEntity(Entity swappedEntity, u32 newRow);

	Vector<EntityRecord> m_entities;  // indexed by entity.id
	Vector<u32>          m_freeIds;   // recycled entity IDs

	// ComponentMask needs a custom hash since std::bitset has no default hash.
	// HashMap alias only takes 2 template args, so use std::unordered_map directly.
	std::unordered_map<ComponentMask, URef<Archetype>, ComponentMaskHash> m_archetypes;

	u32 m_nextId = 1; // 0 is reserved for k_nullEntity

	// Systems — executed in registration order.
	Vector<URef<System>> m_systems;
};
