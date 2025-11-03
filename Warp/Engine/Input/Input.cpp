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
	LOG_DEBUG("Mouse Button and bPressed " + std::to_string(bPressed));

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

void InputEventManager::KeyCallback(KeyCode KeyCode, bool bPressed)
{
	LOG_DEBUG("Key " + KeyCodeToStringMap[KeyCode] + ", bPressed " + std::to_string(bPressed));

	if (bPressed)
	{
		if (m_subbedKeysDOWN.contains(KeyCode))
		{
			for (auto& CallBack : m_subbedKeysDOWN[KeyCode])
			{
				CallBack();
			}
		}
	}
	else
	{
		if (m_subbedKeysUP.contains(KeyCode))
		{
			for (auto& CallBack : m_subbedKeysUP[KeyCode])
			{
				CallBack();
			}
		}
	}
}

void InputEventManager::MouseMoveCallback(int32 x, int32 y)
{
	LOG_DEBUG("Mouse Move Pos (" + std::to_string(x) + ", " + std::to_string(y) + ")");
	if (m_subbedMouseMoveFunc != nullptr)
	{
		m_subbedMouseMoveFunc(x, y);
	}
}

void InputEventManager::SubscribeToKeyUp(KeyCode Code, void (*Func)(void))
{
	m_subbedKeysUP[Code].push_back(Func);
}

void InputEventManager::SubscribeToKeyDown(KeyCode Code, void (*Func)(void))
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