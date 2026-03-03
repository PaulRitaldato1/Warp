#include <Core/ECS/World.h>
#include <Debugging/Assert.h>
#include <Debugging/Logging.h>
#include <cstring>

Entity World::CreateEntity()
{
	Entity entity;

	if (!m_freeIds.empty())
	{
		entity.id = m_freeIds.back();
		m_freeIds.pop_back();
		entity.generation = m_entities[entity.id].generation;
	}
	else
	{
		entity.id = m_nextId++;

		// Grow the entity table if needed.
		if (m_entities.size() <= entity.id)
		{
			m_entities.resize(entity.id + 1);
		}

		entity.generation = 0;
	}

	EntityRecord& record = m_entities[entity.id];
	record.generation = entity.generation;
	record.alive      = true;
	record.archetype  = nullptr;
	record.row        = 0;

	return entity;
}

void World::DestroyEntity(Entity entity)
{
	FATAL_ASSERT(IsAlive(entity), "World::DestroyEntity: entity is not alive or has stale generation");

	EntityRecord& record = m_entities[entity.id];

	// Remove from archetype if it has components.
	if (record.archetype)
	{
		Entity swappedEntity = record.archetype->RemoveEntity(record.row);
		if (swappedEntity != k_nullEntity)
		{
			FixupSwappedEntity(swappedEntity, record.row);
		}
	}

	record.archetype = nullptr;
	record.row       = 0;
	record.alive     = false;
	record.generation++;

	m_freeIds.push_back(entity.id);
}

bool World::IsAlive(Entity entity) const
{
	if (entity.id == 0 || entity.id >= m_entities.size())
	{
		return false;
	}

	const EntityRecord& record = m_entities[entity.id];
	return record.alive && record.generation == entity.generation;
}

Archetype* World::FindOrCreateArchetype(ComponentMask mask)
{
	auto it = m_archetypes.find(mask);
	if (it != m_archetypes.end())
	{
		return it->second.get();
	}

	// Collect ComponentInfo for each set bit from the global registry.
	Vector<ComponentInfo> infos;
	const Vector<ComponentInfo>& registry = detail::ComponentRegistry();

	for (u32 bitIndex = 0; bitIndex < 64; ++bitIndex)
	{
		if (mask.test(bitIndex))
		{
			FATAL_ASSERT(bitIndex < registry.size(),
			             "World::FindOrCreateArchetype: component ID not registered");
			infos.push_back(registry[bitIndex]);
		}
	}

	URef<Archetype> archetype = std::make_unique<Archetype>(mask, infos);
	Archetype* rawPtr = archetype.get();
	m_archetypes[mask] = std::move(archetype);

	return rawPtr;
}

void World::MoveEntity(Entity entity, Archetype* from, u32 fromRow, Archetype* to)
{
	// Add entity to the new archetype (zero-initialized).
	u32 newRow = to->AddEntity(entity);

	// Copy shared components from old archetype to new one.
	ComponentMask sharedMask = from->GetMask() & to->GetMask();
	const Vector<ComponentInfo>& registry = detail::ComponentRegistry();

	for (u32 bitIndex = 0; bitIndex < 64; ++bitIndex)
	{
		if (sharedMask.test(bitIndex))
		{
			size_t componentSize = registry[bitIndex].size;
			byte* srcColumn = from->GetColumnRaw(bitIndex);
			byte* dstColumn = to->GetColumnRaw(bitIndex);

			std::memcpy(dstColumn + newRow * componentSize,
			            srcColumn + fromRow * componentSize,
			            componentSize);
		}
	}

	// Remove entity from old archetype (swap-remove).
	Entity swappedEntity = from->RemoveEntity(fromRow);
	if (swappedEntity != k_nullEntity)
	{
		FixupSwappedEntity(swappedEntity, fromRow);
	}

	// Update the entity record to point to the new location.
	EntityRecord& record = m_entities[entity.id];
	record.archetype = to;
	record.row = newRow;
}

void World::UpdateSystems(f32 deltaTime)
{
	for (URef<System>& system : m_systems)
	{
		system->Update(*this, deltaTime);
	}
}

void World::ShutdownSystems()
{
	for (URef<System>& system : m_systems)
	{
		system->Shutdown();
	}
	m_systems.clear();
}

void World::FixupSwappedEntity(Entity swappedEntity, u32 newRow)
{
	DYNAMIC_ASSERT(swappedEntity.id < m_entities.size(),
	               "World::FixupSwappedEntity: swapped entity ID out of range");

	EntityRecord& swappedRecord = m_entities[swappedEntity.id];
	swappedRecord.row = newRow;
}
