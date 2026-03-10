#pragma once

#include <Common/CommonTypes.h>
#include <Input/Input.h>
#include <Rendering/Cameras/Camera.h>

// ---------------------------------------------------------------------------
// FreeCam — first-person fly camera driven by keyboard + mouse input.
//
// Controls:
//   W / S        — move forward / backward
//   A / D        — strafe left / right
//   Q / E        — move down / up
//   Mouse move   — look (yaw / pitch)
//
// Call Initialize() once after the input system is ready, then call
// Update(deltaTime) every frame to advance position and rebuild matrices.
// ---------------------------------------------------------------------------

class WARP_API FreeCam : public Camera
{
public:
	void Initialize(f32 fovDegrees, f32 aspect, f32 nearZ = 0.1f, f32 farZ = 1000.f);
	void Update(f32 deltaTime) override;
	Mat4 GetViewProjectionMatrix() const override;

	void SetPosition(Vec3 pos)
	{
		m_position = pos;
	}
	void SetAspect(f32 aspect);
	void SetNearPlane(f32 nearZ)
	{
		m_nearZ = nearZ;
		RebuildProjection();
	}
	void SetFarPlane(f32 farZ)
	{
		m_farZ = farZ;
		RebuildProjection();
	}

	const Mat4& GetViewMatrix() const
	{
		return m_view;
	}
	const Mat4& GetProjectionMatrix() const
	{
		return m_proj;
	}

private:
	void RebuildView();
	void RebuildProjection();

	void OnKey(WarpKeyCode key, bool pressed);
	void OnMouseMove(int32 x, int32 y);

	// --- Movement state set by OnKey ---
	bool m_moveForward	= false;
	bool m_moveBackward = false;
	bool m_moveLeft		= false;
	bool m_moveRight	= false;
	bool m_moveUp		= false;
	bool m_moveDown		= false;

	// --- Camera parameters ---
	Vec3 m_position = { 0.f, 0.f, -3.f };
	f32 m_yaw		= 0.f; // degrees, rotation around world Y axis
	f32 m_pitch		= 0.f; // degrees, rotation around local X axis
	f32 m_fov		= 60.f;
	f32 m_aspect	= 16.f / 9.f;
	f32 m_nearZ		= 0.1f;
	f32 m_farZ		= 1000.f;
	f32 m_moveSpeed = 5.f;
	f32 m_lookSpeed = 0.15f; // degrees per pixel

	Mat4 m_view;
	Mat4 m_proj;

	// Owned delegates — must outlive any InputEventManager subscriptions.
	URef<MemberFuncType<FreeCam, WarpKeyCode, bool>> m_keyDelegate;
	URef<MemberFuncType<FreeCam, int32, int32>> m_mouseMoveDelegate;
};
