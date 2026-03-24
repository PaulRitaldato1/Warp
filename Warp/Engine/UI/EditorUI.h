#pragma once

#include <Common/CommonTypes.h>
#include <Core/ECS/Entity.h>

class World;

class WARP_API EditorUI
{
public:
	void BuildUI(World& world);

private:
	void DrawEntityList(World& world);
	void DrawEntityInspector(World& world);
	void DrawEntityCreator(World& world);

	Entity m_selectedEntity = k_nullEntity;
	bool m_showEntityCreator = false;
};
