#include <Input/Input.h>
#include <Debugging/Logging.h>

WARP_API InputEventManager g_InputEventManager;

static const char* GetKeyName(WarpKeyCode code)
{
	const auto it = WarpWarpKeyCodeToStringMap.find(code);
	return it != WarpWarpKeyCodeToStringMap.end() ? it->second.c_str() : "Unknown";
}

static const char* GetMouseButtonName(MouseCode code)
{
	switch (code)
	{
		case BUTTON_LEFT:
			return "Left";
		case BUTTON_RIGHT:
			return "Right";
		case BUTTON_MIDDLE:
			return "Middle";
		default:
			return "Unknown";
	}
}

void InputEventManager::BroadcastKey(WarpKeyCode code, bool bPressed)
{
	LOG_VERBOSE("Key {} {}", GetKeyName(code), bPressed ? "down" : "up");

	m_onKey.Broadcast(code, bPressed);

	if (bPressed)
	{
		m_onKeyDown[code].Broadcast();
	}
	else
	{
		m_onKeyUp[code].Broadcast();
	}
}

void InputEventManager::BroadcastMouseButton(MouseCode code, bool bPressed)
{
	LOG_VERBOSE("Mouse {} {}", GetMouseButtonName(code), bPressed ? "down" : "up");

	m_onMouseButton.Broadcast(code, bPressed);

	if (bPressed)
	{
		m_onMouseDown[code].Broadcast();
	}
	else
	{
		m_onMouseUp[code].Broadcast();
	}
}

void InputEventManager::BroadcastMouseMove(int32 dx, int32 dy)
{
	LOG_VERBOSE("Mouse move {}, {}", dx, dy);

	m_onMouseMove.Broadcast(dx, dy);
}
