#pragma once

#include <Common/CommonTypes.h>
#include <Core/ECS/Entity.h>

class World;
class ResourceManager;
class Renderer;

class WARP_API EditorUI
{
public:
	void SetResourceManager(ResourceManager* resourceManager) { m_resourceManager = resourceManager; }
	void SetRenderer(Renderer* renderer) { m_renderer = renderer; }

	void BuildUI(World& world);

private:
	void DrawEntityList(World& world);
	void DrawEntityInspector(World& world);
	void DrawEntityCreator(World& world);
	void DrawMeshBrowser(World& world);
	void DrawRendererStats();

	// Scans the Resources directory for mesh files and caches the result.
	void RefreshMeshFileList();

	Entity m_selectedEntity = k_nullEntity;
	bool m_showEntityCreator = false;

	ResourceManager* m_resourceManager = nullptr;
	Renderer* m_renderer			   = nullptr;

	// Cached list of mesh file paths found in Resources/.
	Vector<String> m_meshFiles;
	bool m_meshFilesScanned = false;
};
