#include <UI/EditorUI.h>
#include <UI/ComponentDescriptor.h>
#include <Core/ECS/World.h>
#include <imgui.h>

void EditorUI::BuildUI(World& world)
{
	DrawEntityList(world);
	DrawEntityInspector(world);

	if (m_showEntityCreator)
	{
		DrawEntityCreator(world);
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

	Vector<Entity> entities = world.GetAllEntities();
	for (Entity entity : entities)
	{
		ImGui::PushID(static_cast<int>(entity.id));

		// Build a label showing entity ID and which components it has.
		ComponentMask mask = world.GetComponentMask(entity);
		String label = "Entity " + std::to_string(entity.id);

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
