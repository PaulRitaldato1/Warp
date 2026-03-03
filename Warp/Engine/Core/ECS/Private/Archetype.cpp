#include <Core/ECS/Archetype.h>
#include <Debugging/Assert.h>
#include <cstring>

Archetype::Archetype(ComponentMask mask, const Vector<ComponentInfo>& infos)
	: m_mask(mask)
	, m_infos(infos)
{
	m_columns.resize(m_infos.size());
	for (u32 columnIndex = 0; columnIndex < static_cast<u32>(m_infos.size()); ++columnIndex)
	{
		m_idToColumn[m_infos[columnIndex].id] = columnIndex;
	}
}

u32 Archetype::AddEntity(Entity entity)
{
	u32 row = static_cast<u32>(m_entities.size());
	m_entities.push_back(entity);

	// Append zero-initialized bytes for each component column.
	for (u32 columnIndex = 0; columnIndex < static_cast<u32>(m_infos.size()); ++columnIndex)
	{
		size_t componentSize = m_infos[columnIndex].size;
		size_t oldSize = m_columns[columnIndex].size();
		m_columns[columnIndex].resize(oldSize + componentSize, 0);
	}

	return row;
}

Entity Archetype::RemoveEntity(u32 row)
{
	DYNAMIC_ASSERT(row < m_entities.size(), "Archetype::RemoveEntity: row out of range");

	u32 lastRow = static_cast<u32>(m_entities.size()) - 1;
	Entity swappedEntity = k_nullEntity;

	if (row != lastRow)
	{
		// Swap-remove: memcpy the last row's data into the vacated row.
		swappedEntity = m_entities[lastRow];

		for (u32 columnIndex = 0; columnIndex < static_cast<u32>(m_infos.size()); ++columnIndex)
		{
			size_t componentSize = m_infos[columnIndex].size;
			byte* columnData = m_columns[columnIndex].data();
			std::memcpy(columnData + row * componentSize,
			            columnData + lastRow * componentSize,
			            componentSize);
		}

		m_entities[row] = m_entities[lastRow];
	}

	// Pop the last row.
	m_entities.pop_back();
	for (u32 columnIndex = 0; columnIndex < static_cast<u32>(m_infos.size()); ++columnIndex)
	{
		size_t componentSize = m_infos[columnIndex].size;
		m_columns[columnIndex].resize(m_columns[columnIndex].size() - componentSize);
	}

	return swappedEntity;
}

byte* Archetype::GetColumnRaw(u32 componentId)
{
	auto it = m_idToColumn.find(componentId);
	if (it == m_idToColumn.end())
	{
		return nullptr;
	}
	return m_columns[it->second].data();
}

u32 Archetype::GetColumnIndex(u32 componentId) const
{
	auto it = m_idToColumn.find(componentId);
	if (it == m_idToColumn.end())
	{
		return UINT32_MAX;
	}
	return it->second;
}
