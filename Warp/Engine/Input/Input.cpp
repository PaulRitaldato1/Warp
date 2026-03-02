#include <Input/Input.h>
#include <Debugging/Logging.h>

WARP_API InputEventManager g_InputEventManager;

InputEventManager::InputEventManager()
{
	m_mouseDelegate = std::make_unique<MouseDelegate>(this, &InputEventManager::MouseButtonCallback);
	MouseButtonEventManager.Subscribe(m_mouseDelegate.get());

	m_mouseMoveDelegate = std::make_unique<MouseMoveDelegate>(this, &InputEventManager::MouseMoveCallback);
	MouseMoveEventManager.Subscribe(m_mouseMoveDelegate.get());

	m_keyDelegate = std::make_unique<KeyDelegate>(this, &InputEventManager::KeyCallback);
	KeyPressedEventManager.Subscribe(m_keyDelegate.get());

	m_subbedMouseMoveFunc = nullptr;
}

void InputEventManager::MouseButtonCallback(MouseCode mb, bool bPressed)
{
	LOG_DEBUG("Mouse Button and bPressed {}", bPressed);

	if (bPressed)
	{
		if (m_subbedButtonsDOWN.contains(mb))
		{
			for (auto& Callback : m_subbedButtonsDOWN[mb])
			{
				Callback();
			}
		}
	}
	else
	{
		if (m_subbedButtonsUP.contains(mb))
		{
			for (auto& Callback : m_subbedButtonsUP[mb])
			{
				Callback();
			}
		}
	}
}

void InputEventManager::KeyCallback(WarpKeyCode WarpKeyCode, bool bPressed)
{
	LOG_DEBUG("Key {}, bPressed {}", WarpWarpKeyCodeToStringMap[WarpKeyCode], bPressed);

	if (bPressed)
	{
		if (m_subbedKeysDOWN.contains(WarpKeyCode))
		{
			for (auto& CallBack : m_subbedKeysDOWN[WarpKeyCode])
			{
				CallBack();
			}
		}
	}
	else
	{
		if (m_subbedKeysUP.contains(WarpKeyCode))
		{
			for (auto& CallBack : m_subbedKeysUP[WarpKeyCode])
			{
				CallBack();
			}
		}
	}
}

void InputEventManager::MouseMoveCallback(int32 x, int32 y)
{
	if (m_subbedMouseMoveFunc != nullptr)
	{
		m_subbedMouseMoveFunc(x, y);
	}
}

void InputEventManager::SubscribeToKeyUp(WarpKeyCode Code, void (*Func)(void))
{
	m_subbedKeysUP[Code].push_back(Func);
}

void InputEventManager::SubscribeToKeyDown(WarpKeyCode Code, void (*Func)(void))
{
	m_subbedKeysDOWN[Code].push_back(Func);
}

void InputEventManager::SubscribeToMouseUp(MouseCode Code, void (*Func)(void))
{
	m_subbedButtonsUP[Code].push_back(Func);
}

void InputEventManager::SubscribeToMouseDown(MouseCode Code, void (*Func)(void))
{
	m_subbedButtonsDOWN[Code].push_back(Func);
}

void InputEventManager::SubscribeToMouseMove(void (*Func)(int32, int32))
{
	m_subbedMouseMoveFunc = Func;
}