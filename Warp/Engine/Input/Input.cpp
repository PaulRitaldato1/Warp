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

    if(bPressed)
    {
        if(m_subbedButtonsDOWN[static_cast<u32>(mb)] != nullptr)
        {
            m_subbedButtonsDOWN[static_cast<u32>(mb)](); 
        }
    }
    else
    {
        if(m_subbedButtonsUP[static_cast<u32>(mb)] != nullptr)
        {
            m_subbedButtonsUP[static_cast<u32>(mb)](); 
        }
    }
}

void InputEventManager::KeyCallback(KeyCode keycode, bool bPressed)
{
    LOG_DEBUG("Key " + KeyCodeToStringMap[keycode] + ", bPressed " + std::to_string(bPressed));

    if(bPressed)
    {
        if(m_subbedKeysDOWN.contains(keycode))
        {
            m_subbedKeysDOWN[keycode]();
        }
    }
    else 
    {
        if(m_subbedKeysUP.contains(keycode))
        {
            m_subbedKeysUP[keycode]();
        }
    }
}

void InputEventManager::MouseMoveCallback(int32 x, int32 y)
{
    LOG_DEBUG("Mouse Move Pos (" + std::to_string(x) + ", " + std::to_string(y) + ")");
    if(m_subbedMouseMoveFunc != nullptr)
    {
        m_subbedMouseMoveFunc(x, y);
    }
}

void InputEventManager::SubscribeToKeyUp(KeyCode codeToSubTo, void (*func)(void))
{
    m_subbedKeysUP[codeToSubTo] = func;
}

void InputEventManager::SubscribeToKeyDown(KeyCode codeToSubTo, void (*func)(void))
{
    m_subbedKeysDOWN[codeToSubTo] = func;
}

void InputEventManager::SubscribeToMouseUp(MouseCode codeToSubTo, void (*func)(void))
{
    m_subbedButtonsUP[static_cast<u32>(codeToSubTo)] = func;
}

void InputEventManager::SubscribeToMouseDown(MouseCode codeToSubTo, void (*func)(void))
{
    m_subbedButtonsDOWN[static_cast<u32>(codeToSubTo)] = func;
}

void InputEventManager::SubscribeToMouseMove(void (*func)(int32, int32))
{
    m_subbedMouseMoveFunc = func;
}