#include <UI/EditorUI.h>
#include <UI/ComponentDescriptor.h>
#include <Core/ECS/World.h>
#include <Core/ECS/Components/MeshComponent.h>
#include <Rendering/Resource/ResourceManager.h>
#include <Rendering/Renderer/Renderer.h>
#include <imgui.h>
#include <filesystem>

void EditorUI::BuildUI(World& world)
{
	DrawEntityList(world);
	DrawEntityInspector(world);
	DrawRendererStats();

	if (m_showEntityCreator)
	{
		DrawEntityCreator(world);
	}
}

// ---------------------------------------------------------------------------
// Renderer Stats — frustum culling counts for the last frame.
// ---------------------------------------------------------------------------
void EditorUI::DrawRendererStats()
{
	if (!ImGui::Begin("Renderer"))
	{
		ImGui::End();
		return;
	}

	if (!m_renderer)
	{
		ImGui::TextDisabled("No renderer.");
		ImGui::End();
		return;
	}

	const Renderer::CullStats stats = m_renderer->GetCullStats();
	const u32 visible				= stats.tested - stats.culled;

	ImGui::SeparatorText("Frustum Culling");
	ImGui::Text("Tested:  %u", stats.tested);
	ImGui::Text("Visible: %u", visible);
	ImGui::Text("Culled:  %u", stats.culled);

	// Counts entities, not submeshes, and culled includes shadow casters that were
	// dropped from the camera pass but still drawn into the shadow map.
	const f32 percent = stats.tested > 0 ? (100.f * static_cast<f32>(stats.culled) / static_cast<f32>(stats.tested)) : 0.f;
	ImGui::Text("Culled %%:  %.1f", percent);

	ImGui::End();
}

// ---------------------------------------------------------------------------
// Mesh file scanning — caches list of .gltf/.glb files in Resources/.
// ---------------------------------------------------------------------------
void EditorUI::RefreshMeshFileList()
{
	m_meshFiles.clear();
	m_meshFilesScanned = true;

	std::filesystem::path resourcesDir = "Resources";
	if (!std::filesystem::exists(resourcesDir))
	{
		return;
	}

	for (const auto& entry : std::filesystem::recursive_directory_iterator(resourcesDir))
	{
		if (!entry.is_regular_file())
		{
			continue;
		}

		String extension = entry.path().extension().string();
		if (extension == ".gltf" || extension == ".glb")
		{
			// Store as forward-slash relative path (matches how MeshComponent paths are used).
			String path = entry.path().generic_string();
			m_meshFiles.push_back(path);
		}
	}
}

// ---------------------------------------------------------------------------
// Entity List — shows all entities, allows selection and creation/deletion.
// ---------------------------------------------------------------------------
void EditorUI::DrawEntityList(World& world)
{
	if (!ImGui::Begin("Entities"))
	{
		ImGui::End();
		return;
	}

	if (ImGui::Button("New Entity"))
	{
		m_showEntityCreator = true;
	}

	ImGui::Separator();

	// Reused across frames so a large world does not allocate here every frame.
	m_entityCache.clear();
	world.GetAllEntities(m_entityCache);

	// Only rows actually on screen are built. Without the clipper every entity pays
	// for a label string whether visible or not, which dominates at 10k entities.
	ImGuiListClipper clipper;
	clipper.Begin(static_cast<int>(m_entityCache.size()));

	while (clipper.Step())
	{
		for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index)
		{
			const Entity entity = m_entityCache[index];

			ImGui::PushID(static_cast<int>(entity.id));

			// Build a label showing entity ID and which components it has.
			ComponentMask mask = world.GetComponentMask(entity);
			String label	   = "Entity " + std::to_string(entity.id);

			// Append component short names.
			const auto& descriptors = GetComponentDescriptors();
			for (const auto& [componentId, descriptor] : descriptors)
			{
				if (mask.test(componentId))
				{
					label += "  [";
					label += descriptor.name;
					label += "]";
				}
			}

			bool isSelected = (m_selectedEntity == entity);
			if (ImGui::Selectable(label.c_str(), isSelected))
			{
				m_selectedEntity = entity;
			}

			ImGui::PopID();
		}
	}

	ImGui::End();
}

// ---------------------------------------------------------------------------
// Entity Inspector — shows components of the selected entity with edit widgets.
// ---------------------------------------------------------------------------
void EditorUI::DrawEntityInspector(World& world)
{
	if (!ImGui::Begin("Inspector"))
	{
		ImGui::End();
		return;
	}

	if (m_selectedEntity == k_nullEntity || !world.IsAlive(m_selectedEntity))
	{
		ImGui::TextDisabled("No entity selected");
		ImGui::End();
		return;
	}

	ImGui::Text("Entity %u (gen %u)", m_selectedEntity.id, m_selectedEntity.generation);
	ImGui::Separator();

	ComponentMask mask = world.GetComponentMask(m_selectedEntity);
	const auto& descriptors = GetComponentDescriptors();
	u32 meshComponentId = ComponentID<MeshComponent>::Get();

	// Draw each component the entity has.
	for (const auto& [componentId, descriptor] : descriptors)
	{
		if (!mask.test(componentId))
		{
			continue;
		}

		bool open = ImGui::TreeNodeEx(descriptor.name, ImGuiTreeNodeFlags_DefaultOpen);

		// Right-click context menu to remove the component.
		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Remove Component"))
			{
				descriptor.removeFromEntity(world, m_selectedEntity);
				ImGui::EndPopup();
				if (open) ImGui::TreePop();
				// Mask changed — break out and re-draw next frame.
				break;
			}
			ImGui::EndPopup();
		}

		if (open)
		{
			// For MeshComponent, draw the mesh browser before the generic fields.
			if (componentId == meshComponentId && m_resourceManager)
			{
				DrawMeshBrowser(world);
			}

			void* componentData = world.GetComponentRaw(componentId, m_selectedEntity);
			descriptor.drawUI(componentData);
			ImGui::TreePop();
		}
	}

	ImGui::Separator();

	// "Add Component" combo — only shows components the entity doesn't have.
	if (ImGui::BeginCombo("Add Component", "Select..."))
	{
		for (const auto& [componentId, descriptor] : descriptors)
		{
			if (!mask.test(componentId))
			{
				if (ImGui::Selectable(descriptor.name))
				{
					descriptor.addToEntity(world, m_selectedEntity);
				}
			}
		}
		ImGui::EndCombo();
	}

	ImGui::Separator();

	// Delete entity button.
	if (ImGui::Button("Delete Entity"))
	{
		world.DestroyEntity(m_selectedEntity);
		m_selectedEntity = k_nullEntity;
	}

	ImGui::End();
}

// ---------------------------------------------------------------------------
// Mesh Browser — dropdown to pick from disk meshes or built-in geometry.
// ---------------------------------------------------------------------------
void EditorUI::DrawMeshBrowser(World& world)
{
	if (!m_meshFilesScanned)
	{
		RefreshMeshFileList();
	}

	MeshComponent& mesh = world.GetComponent<MeshComponent>(m_selectedEntity);

	// Determine the current label for the combo.
	const char* currentLabel = mesh.HasPath() ? mesh.GetPath().c_str() : "(none)";

	if (ImGui::BeginCombo("Mesh", currentLabel))
	{
		// Built-in geometry section.
		ImGui::TextDisabled("Built-in Geometry");
		ImGui::Separator();

		if (ImGui::Selectable("  Plane"))
		{
			u32 handle = m_resourceManager->CreatePlane(10.f, 10.f);
			mesh.meshHandle = handle;
			mesh.ClearPath();
		}
		if (ImGui::Selectable("  Box"))
		{
			u32 handle = m_resourceManager->CreateBox(1.f, 1.f, 1.f);
			mesh.meshHandle = handle;
			mesh.ClearPath();
		}

		// Mesh files from Resources/ section.
		if (!m_meshFiles.empty())
		{
			ImGui::Separator();
			ImGui::TextDisabled("Resource Files");
			ImGui::Separator();

			for (const String& filePath : m_meshFiles)
			{
				bool isSelected = (mesh.GetPath() == filePath);
				if (ImGui::Selectable(filePath.c_str(), isSelected))
				{
					m_resourceManager->AssignMesh(mesh, filePath.c_str());
				}
			}
		}

		ImGui::EndCombo();
	}

	// Refresh button in case files were added at runtime.
	ImGui::SameLine();
	if (ImGui::SmallButton("Refresh"))
	{
		RefreshMeshFileList();
	}
}

// ---------------------------------------------------------------------------
// Entity Creator — pick components to attach, then create.
// ---------------------------------------------------------------------------
void EditorUI::DrawEntityCreator(World& world)
{
	if (!ImGui::Begin("Create Entity", &m_showEntityCreator))
	{
		ImGui::End();
		return;
	}

	// Track which components to add — using a static map so it persists across frames.
	static HashMap<u32, bool> componentChecks;
	const auto& descriptors = GetComponentDescriptors();

	// Initialize any new descriptors that appeared.
	for (const auto& [componentId, descriptor] : descriptors)
	{
		if (componentChecks.find(componentId) == componentChecks.end())
		{
			componentChecks[componentId] = false;
		}
	}

	for (const auto& [componentId, descriptor] : descriptors)
	{
		ImGui::Checkbox(descriptor.name, &componentChecks[componentId]);
	}

	if (ImGui::Button("Create"))
	{
		Entity entity = world.CreateEntity();

		for (const auto& [componentId, descriptor] : descriptors)
		{
			if (componentChecks[componentId])
			{
				descriptor.addToEntity(world, entity);
			}
		}

		m_selectedEntity    = entity;
		m_showEntityCreator = false;

		// Reset checkboxes.
		for (auto& [id, checked] : componentChecks)
		{
			checked = false;
		}
	}

	ImGui::SameLine();
	if (ImGui::Button("Cancel"))
	{
		m_showEntityCreator = false;
	}

	ImGui::End();
}
