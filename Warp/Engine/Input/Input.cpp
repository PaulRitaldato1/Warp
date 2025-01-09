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

	// for(auto func : m_subbedButtonsUP)
	// {
	//     func = nullptr;
	// }

	// for(auto func : m_subbedButtonsDOWN)
	// {
	//     func = nullptr;
	// }
}

void InputEventManager::MouseButtonCallback(MouseCode mb, bool bPressed)
{
	LOG_DEBUG("Mouse Button and bPressed " + std::to_string(bPressed));

	if (bPressed)
	{
		if (m_subbedButtonsDOWN[static_cast<u32>(mb)] != nullptr)
		{
			m_subbedButtonsDOWN[static_cast<u32>(mb)]();
		}
	}
	else
	{
		if (m_subbedButtonsUP[static_cast<u32>(mb)] != nullptr)
		{
			m_subbedButtonsUP[static_cast<u32>(mb)]();
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
			m_subbedKeysDOWN[KeyCode]();
		}
	}
	else
	{
		if (m_subbedKeysUP.contains(KeyCode))
		{
			m_subbedKeysUP[KeyCode]();
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
	m_subbedKeysUP[Code] = Func;
}

void InputEventManager::SubscribeToKeyDown(KeyCode Code, void (*Func)(void))
{
	m_subbedKeysDOWN[Code] = Func;
}

void InputEventManager::SubscribeToMouseUp(MouseCode Code, void (*Func)(void))
{
	m_subbedButtonsUP[static_cast<u32>(Code)] = Func;
}

void InputEventManager::SubscribeToMouseDown(MouseCode Code, void (*Func)(void))
{
	m_subbedButtonsDOWN[static_cast<u32>(Code)] = Func;
}

void InputEventManager::SubscribeToMouseMove(void (*Func)(int32, int32))
{
	m_subbedMouseMoveFunc = Func;
}