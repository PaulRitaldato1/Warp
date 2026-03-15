#include <Core/ECS/Systems/FreeCamSystem.h>
#include <Core/ECS/World.h>
#include <Core/ECS/Components/TransformComponent.h>
#include <Core/ECS/Components/CameraComponent.h>
#include <cmath>

using namespace DirectX;

void FreeCamSystem::Init(World& /*world*/)
{
	m_keyDelegate       = std::make_unique<MemberFuncType<FreeCamSystem, WarpKeyCode, bool>>(this, &FreeCamSystem::OnKey);
	m_mouseMoveDelegate = std::make_unique<MemberFuncType<FreeCamSystem, int32, int32>>(this, &FreeCamSystem::OnMouseMove);

	g_InputEventManager.SubscribeToKeyEvents(m_keyDelegate.get());
	g_InputEventManager.SubscribeToMouseMoveEvents(m_mouseMoveDelegate.get());
}

void FreeCamSystem::Update(World& world, f32 deltaTime)
{
	world.Each<TransformComponent, CameraComponent>(
		[&](Entity entity, TransformComponent& transform, CameraComponent& cam)
		{
			if (!cam.isActive)
			{
				return;
			}

			// Build forward and right vectors from yaw/pitch.
			const f32 yawRad   = XMConvertToRadians(m_yaw);
			const f32 pitchRad = XMConvertToRadians(m_pitch);

			SimdVec forward = XMVectorSet(
				std::cos(pitchRad) * std::sin(yawRad),
				-std::sin(pitchRad),
				std::cos(pitchRad) * std::cos(yawRad),
				0.f);

			SimdVec worldUp = XMVectorSet(0.f, 1.f, 0.f, 0.f);
			SimdVec right   = XMVector3Normalize(XMVector3Cross(worldUp, forward));

			// Apply movement.
			SimdVec pos   = XMLoadFloat3(&transform.position);
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

			XMStoreFloat3(&transform.position, pos);

			// Rebuild view matrix.
			SimdVec eye    = XMLoadFloat3(&transform.position);
			SimdVec target = XMVectorAdd(eye, forward);
			SimdMat view   = XMMatrixLookAtLH(eye, target, worldUp);

			// Rebuild projection matrix.
			SimdMat proj = XMMatrixPerspectiveFovLH(
				XMConvertToRadians(cam.fovY), cam.aspect, cam.nearPlane, cam.farPlane);

			// Store combined view-projection.
			XMStoreFloat4x4(&cam.viewProj, XMMatrixMultiply(view, proj));
		});
}

void FreeCamSystem::Shutdown()
{
	m_keyDelegate.reset();
	m_mouseMoveDelegate.reset();
}

void FreeCamSystem::OnKey(WarpKeyCode key, bool pressed)
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

void FreeCamSystem::OnMouseMove(int32 dx, int32 dy)
{
	m_yaw   += static_cast<f32>(dx) * m_lookSpeed;
	m_pitch += static_cast<f32>(dy) * m_lookSpeed;

	if (m_pitch > 89.f)
	{
		m_pitch = 89.f;
	}
	if (m_pitch < -89.f)
	{
		m_pitch = -89.f;
	}
}
