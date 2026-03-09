#pragma once

#include <Core/ECS/Component.h>

class WARP_API Archetype
{
public:
	Archetype(ComponentMask mask, const Vector<ComponentInfo>& infos);

	ComponentMask GetMask() const
	{
		return m_mask;
	}
	u32 GetEntityCount() const
	{
		return static_cast<u32>(m_entities.size());
	}
	const Vector<Entity>& GetEntities() const
	{
		return m_entities;
	}

	// Add an entity with zero-initialized component data.
	// Returns its row index within this archetype.
	u32 AddEntity(Entity entity);

	// Remove entity at row via swap-remove with the last row.
	// Returns the entity that was swapped into the vacated slot,
	// or k_nullEntity if the removed entity was the last one.
	Entity RemoveEntity(u32 row);

	// Typed access to a component column.
	template <IsComponent T> T* GetColumn()
	{
		u32 componentId = ComponentID<T>::Get();
		auto it			= m_idToColumn.find(componentId);
		if (it == m_idToColumn.end())
		{
			return nullptr;
		}
		return reinterpret_cast<T*>(m_columns[it->second].data());
	}

	template <IsComponent T> const T* GetColumn() const
	{
		u32 componentId = ComponentID<T>::Get();
		auto it			= m_idToColumn.find(componentId);
		if (it == m_idToColumn.end())
		{
			return nullptr;
		}
		return reinterpret_cast<const T*>(m_columns[it->second].data());
	}

	// Raw byte access by component ID (for type-erased archetype migration).
	byte* GetColumnRaw(u32 componentId);

	// Returns the column index for a given component ID, or UINT32_MAX if not found.
	u32 GetColumnIndex(u32 componentId) const;

private:
	ComponentMask m_mask;
	Vector<ComponentInfo> m_infos;	// one per component type in this archetype
	Vector<Vector<byte>> m_columns; // m_columns[i] has m_infos[i].size * entityCount bytes
	Vector<Entity> m_entities;		// parallel to rows
	HashMap<u32, u32> m_idToColumn; // componentId -> column index
};
