#include <Rendering/Cameras/FreeCam.h>
#include <cmath>

using namespace DirectX;

void FreeCam::Initialize(f32 fovDegrees, f32 aspect, f32 nearZ, f32 farZ)
{
	m_fov	 = fovDegrees;
	m_aspect = aspect;
	m_nearZ	 = nearZ;
	m_farZ	 = farZ;

	m_keyDelegate		= std::make_unique<MemberFuncType<FreeCam, WarpKeyCode, bool>>(this, &FreeCam::OnKey);
	m_mouseMoveDelegate = std::make_unique<MemberFuncType<FreeCam, int32, int32>>(this, &FreeCam::OnMouseMove);

	g_InputEventManager.SubscribeToKeyEvents(m_keyDelegate.get());
	g_InputEventManager.SubscribeToMouseMoveEvents(m_mouseMoveDelegate.get());

	RebuildProjection();
	RebuildView();
}

void FreeCam::Update(f32 deltaTime)
{
	const f32 yawRad   = XMConvertToRadians(m_yaw);
	const f32 pitchRad = XMConvertToRadians(m_pitch);

	SimdVec forward = XMVectorSet(std::cos(pitchRad) * std::sin(yawRad), -std::sin(pitchRad),
								  std::cos(pitchRad) * std::cos(yawRad), 0.f);

	SimdVec worldUp = XMVectorSet(0.f, 1.f, 0.f, 0.f);
	SimdVec right	= XMVector3Normalize(XMVector3Cross(worldUp, forward));

	SimdVec pos	  = XMLoadFloat3(&m_position);
	const f32 spd = m_moveSpeed * deltaTime;

	if (m_moveForward)
	{
		pos = XMVectorAdd(pos, XMVectorScale(forward, spd));
	}
	if (m_moveBackward)
	{
		pos = XMVectorAdd(pos, XMVectorScale(forward, -spd));
	}
	if (m_moveRight)
	{
		pos = XMVectorAdd(pos, XMVectorScale(right, spd));
	}
	if (m_moveLeft)
	{
		pos = XMVectorAdd(pos, XMVectorScale(right, -spd));
	}
	if (m_moveUp)
	{
		pos = XMVectorAdd(pos, XMVectorScale(worldUp, spd));
	}
	if (m_moveDown)
	{
		pos = XMVectorAdd(pos, XMVectorScale(worldUp, -spd));
	}

	XMStoreFloat3(&m_position, pos);

	RebuildView();
}

void FreeCam::SetAspect(f32 aspect)
{
	m_aspect = aspect;
	RebuildProjection();
}

Mat4 FreeCam::GetViewProjectionMatrix() const
{
	SimdMat v = XMLoadFloat4x4(&m_view);
	SimdMat p = XMLoadFloat4x4(&m_proj);

	// HLSL's column-major storage already transposes the raw bytes on upload,
	// so mul(M, v) in the shader is correct without an explicit transpose here.
	Mat4 result;
	XMStoreFloat4x4(&result, XMMatrixMultiply(v, p));
	return result;
}

void FreeCam::RebuildView()
{
	const f32 yawRad   = XMConvertToRadians(m_yaw);
	const f32 pitchRad = XMConvertToRadians(m_pitch);

	SimdVec forward = XMVectorSet(std::cos(pitchRad) * std::sin(yawRad), -std::sin(pitchRad),
								  std::cos(pitchRad) * std::cos(yawRad), 0.f);

	SimdVec eye	   = XMLoadFloat3(&m_position);
	SimdVec target = XMVectorAdd(eye, forward);
	SimdVec up	   = XMVectorSet(0.f, 1.f, 0.f, 0.f);

	XMStoreFloat4x4(&m_view, XMMatrixLookAtLH(eye, target, up));
}

void FreeCam::RebuildProjection()
{
	XMStoreFloat4x4(&m_proj, XMMatrixPerspectiveFovLH(XMConvertToRadians(m_fov), m_aspect, m_nearZ, m_farZ));
}

void FreeCam::OnKey(WarpKeyCode key, bool pressed)
{
	switch (key)
	{
		case KEY_W:
		{
			m_moveForward = pressed;
			break;
		}
		case KEY_S:
		{
			m_moveBackward = pressed;
			break;
		}
		case KEY_A:
		{
			m_moveLeft = pressed;
			break;
		}
		case KEY_D:
		{
			m_moveRight = pressed;
			break;
		}
		case KEY_Q:
		{
			m_moveDown = pressed;
			break;
		}
		case KEY_E:
		{
			m_moveUp = pressed;
			break;
		}
		default:
		{
			break;
		}
	}
}

void FreeCam::OnMouseMove(int32 dx, int32 dy)
{
	// dx/dy are raw relative deltas from WM_INPUT — no cursor-edge clamping.
	m_yaw   += static_cast<f32>(dx) * m_lookSpeed;
	m_pitch += static_cast<f32>(dy) * m_lookSpeed;

	// Clamp pitch to avoid gimbal flip; yaw is uncapped (full 360° rotation).
	if (m_pitch > 89.f)  m_pitch =  89.f;
	if (m_pitch < -89.f) m_pitch = -89.f;
}
