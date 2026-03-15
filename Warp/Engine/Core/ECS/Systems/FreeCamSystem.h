#pragma once

#include <Common/CommonTypes.h>
#include <Core/ECS/System.h>
#include <Input/Input.h>

// ---------------------------------------------------------------------------
// FreeCamSystem — first-person fly camera driven by keyboard + mouse input.
//
// Operates on entities with <TransformComponent, CameraComponent>.
// Only updates the entity whose CameraComponent::isActive is true.
//
// Controls:
//   W / S        — move forward / backward
//   A / D        — strafe left / right
//   Q / E        — move down / up
//   Mouse move   — look (yaw / pitch)
// ---------------------------------------------------------------------------

class WARP_API FreeCamSystem : public System
{
public:
	void Init(World& world) override;
	void Update(World& world, f32 deltaTime) override;
	void Shutdown() override;

	void SetYaw(f32 yaw)     { m_yaw = yaw; }
	void SetPitch(f32 pitch) { m_pitch = pitch; }

private:
	void OnKey(WarpKeyCode key, bool pressed);
	void OnMouseMove(int32 dx, int32 dy);

	// Movement state set by OnKey
	bool m_moveForward  = false;
	bool m_moveBackward = false;
	bool m_moveLeft     = false;
	bool m_moveRight    = false;
	bool m_moveUp       = false;
	bool m_moveDown     = false;

	// Euler angles — controller state, not camera data
	f32 m_yaw       = 0.f;
	f32 m_pitch     = 0.f;
	f32 m_moveSpeed = 5.f;
	f32 m_lookSpeed = 0.15f; // degrees per pixel

	// Owned delegates — must outlive InputEventManager subscriptions.
	URef<MemberFuncType<FreeCamSystem, WarpKeyCode, bool>> m_keyDelegate;
	URef<MemberFuncType<FreeCamSystem, int32, int32>> m_mouseMoveDelegate;
};
