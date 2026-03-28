#include <UI/ComponentDescriptor.h>
#include <Core/ECS/Components/TransformComponent.h>
#include <Core/ECS/Components/CameraComponent.h>
#include <Core/ECS/Components/MeshComponent.h>
#include <Core/ECS/Components/LightComponent.h>
#include <Core/ECS/Components/OrbitComponent.h>
#include <Core/ECS/Components/SkyComponent.h>
#include <imgui.h>

// ---------------------------------------------------------------------------
// TransformComponent
// ---------------------------------------------------------------------------
template <>
struct ComponentUI<TransformComponent>
{
	static void Draw(TransformComponent& transform)
	{
		ImGui::DragFloat3("Position", &transform.position.x, 0.1f);

		// Display rotation as Euler degrees for easier editing.
		using namespace DirectX;
		SimdVec quat = XMLoadFloat4(&transform.rotation);

		// Decompose quaternion to Euler (pitch, yaw, roll).
		// Extract angles via matrix decomposition for stability.
		SimdMat rotMatrix = XMMatrixRotationQuaternion(quat);
		Vec3 euler;
		euler.x = XMConvertToDegrees(asinf(-XMVectorGetY(rotMatrix.r[2])));               // pitch
		euler.y = XMConvertToDegrees(atan2f(XMVectorGetX(rotMatrix.r[2]),
		                                    XMVectorGetZ(rotMatrix.r[2])));                // yaw
		euler.z = XMConvertToDegrees(atan2f(XMVectorGetX(rotMatrix.r[1]),
		                                    XMVectorGetX(rotMatrix.r[0])));                // roll

		if (ImGui::DragFloat3("Rotation (deg)", &euler.x, 0.5f))
		{
			SimdVec newQuat = XMQuaternionRotationRollPitchYaw(
				XMConvertToRadians(euler.x),
				XMConvertToRadians(euler.y),
				XMConvertToRadians(euler.z));
			XMStoreFloat4(&transform.rotation, newQuat);
		}

		ImGui::DragFloat3("Scale", &transform.scale.x, 0.01f, 0.001f, 100.f);
	}

	static inline bool registered = RegisterComponentUI<TransformComponent>("Transform", &Draw);
};

// ---------------------------------------------------------------------------
// CameraComponent
// ---------------------------------------------------------------------------
template <>
struct ComponentUI<CameraComponent>
{
	static void Draw(CameraComponent& camera)
	{
		ImGui::Checkbox("Active", &camera.isActive);
		ImGui::DragFloat("FOV Y", &camera.fovY, 0.5f, 1.f, 179.f);
		ImGui::DragFloat("Aspect", &camera.aspect, 0.01f, 0.1f, 10.f);
		ImGui::DragFloat("Near Plane", &camera.nearPlane, 0.01f, 0.001f, 100.f);
		ImGui::DragFloat("Far Plane", &camera.farPlane, 1.f, 1.f, 100000.f);
	}

	static inline bool registered = RegisterComponentUI<CameraComponent>("Camera", &Draw);
};

// ---------------------------------------------------------------------------
// MeshComponent
// ---------------------------------------------------------------------------
template <>
struct ComponentUI<MeshComponent>
{
	static void Draw(MeshComponent& mesh)
	{
		ImGui::InputText("Path", mesh.path, sizeof(mesh.path));

		bool visible    = mesh.HasRenderFlag(RenderFlags_Visible);
		bool castShadow = mesh.HasRenderFlag(RenderFlags_CastShadow);
		bool receiveFog = mesh.HasRenderFlag(RenderFlags_ReceiveFog);
		bool unlit      = mesh.HasRenderFlag(RenderFlags_Unlit);

		if (ImGui::Checkbox("Visible", &visible))
		{
			visible ? mesh.SetRenderFlag(RenderFlags_Visible) : mesh.ClearRenderFlag(RenderFlags_Visible);
		}
		if (ImGui::Checkbox("Cast Shadow", &castShadow))
		{
			castShadow ? mesh.SetRenderFlag(RenderFlags_CastShadow) : mesh.ClearRenderFlag(RenderFlags_CastShadow);
		}
		if (ImGui::Checkbox("Receive Fog", &receiveFog))
		{
			receiveFog ? mesh.SetRenderFlag(RenderFlags_ReceiveFog) : mesh.ClearRenderFlag(RenderFlags_ReceiveFog);
		}
		if (ImGui::Checkbox("Unlit", &unlit))
		{
			unlit ? mesh.SetRenderFlag(RenderFlags_Unlit) : mesh.ClearRenderFlag(RenderFlags_Unlit);
		}

		ImGui::Text("Handle: %u", mesh.meshHandle);
	}

	static inline bool registered = RegisterComponentUI<MeshComponent>("Mesh", &Draw);
};

// ---------------------------------------------------------------------------
// LightComponent
// ---------------------------------------------------------------------------
template <>
struct ComponentUI<LightComponent>
{
	static void Draw(LightComponent& light)
	{
		const char* typeNames[] = { "Point", "Directional", "Spot" };
		int typeIndex = static_cast<int>(light.type);
		if (ImGui::Combo("Type", &typeIndex, typeNames, 3))
		{
			light.type = static_cast<LightType>(typeIndex);
		}

		ImGui::ColorEdit3("Color", &light.color.x);
		ImGui::DragFloat("Intensity", &light.intensity, 0.1f, 0.f, 1000.f);

		if (light.type != LightType::Directional)
		{
			ImGui::DragFloat("Range", &light.range, 0.1f, 0.f, 1000.f);
		}

		if (light.type == LightType::Spot)
		{
			ImGui::DragFloat("Inner Cone", &light.innerConeAngle, 0.5f, 0.f, 90.f);
			ImGui::DragFloat("Outer Cone", &light.outerConeAngle, 0.5f, 0.f, 90.f);
		}

		ImGui::Checkbox("Cast Shadows", &light.castShadows);
	}

	static inline bool registered = RegisterComponentUI<LightComponent>("Light", &Draw);
};

// ---------------------------------------------------------------------------
// OrbitComponent
// ---------------------------------------------------------------------------
template <>
struct ComponentUI<OrbitComponent>
{
	static void Draw(OrbitComponent& orbit)
	{
		ImGui::DragFloat("Radius", &orbit.radius, 0.1f, 0.f, 500.f);
		ImGui::DragFloat("Speed", &orbit.speed, 0.01f, -10.f, 10.f);
		ImGui::DragFloat("Height", &orbit.height, 0.1f, -500.f, 500.f);
		ImGui::DragFloat("Phase Offset", &orbit.phaseOffset, 0.01f, 0.f, 6.283f);
	}

	static inline bool registered = RegisterComponentUI<OrbitComponent>("Orbit", &Draw);
};

// ---------------------------------------------------------------------------
// SkyComponent
// ---------------------------------------------------------------------------
template <>
struct ComponentUI<SkyComponent>
{
	static void Draw(SkyComponent& sun)
	{
		ImGui::SeparatorText("Sky");
		ImGui::ColorEdit3("Zenith Color", &sun.skyColorZenith.x);
		ImGui::ColorEdit3("Horizon Color", &sun.skyColorHorizon.x);
		ImGui::DragFloat("Horizon Sharpness", &sun.horizonSharpness, 0.01f, 0.01f, 2.0f);

		ImGui::SeparatorText("Ground");
		ImGui::ColorEdit3("Ground Color", &sun.groundColor.x);
		ImGui::DragFloat("Ground Fade", &sun.groundFade, 0.1f, 0.1f, 20.0f);

		ImGui::SeparatorText("Sun Disc");
		ImGui::ColorEdit3("Sun Color", &sun.sunColor.x);
		ImGui::DragFloat("Sun Intensity", &sun.sunIntensity, 0.1f, 0.0f, 50.0f);
		ImGui::DragFloat("Sun Disc Size", &sun.sunDiscSize, 0.0001f, 0.99f, 1.0f, "%.4f");

		ImGui::SeparatorText("Info");
		ImGui::TextDisabled("Sun direction comes from this entity's Transform rotation.");
	}

	static inline bool registered = RegisterComponentUI<SkyComponent>("Sky", &Draw);
};
