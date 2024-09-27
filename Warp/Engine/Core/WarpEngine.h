#pragma once

#include <Common/CommonTypes.h>

// class IRenderer;
class IWindow;
struct UserApplicationBase;

struct EngineDesc
{
    int16 WindowStartPosX;
    int16 WindowStartPosY;

    int16 WindowWidth;
    int16 WindowHeight;

    String Name;
};

class WARP_API WarpEngine
{
public:
    WarpEngine(UserApplicationBase* App);

    bool Run();

private:

    bool m_bIsRunning;
    bool m_bIsSuspended;
    bool m_bIsFullScreen;
    bool m_bMinimized;
    bool m_bMaximized;
    bool m_bResizing;

    f64 m_lastTime;

    // URef<IRenderer> m_renderer;
    Ref<IWindow> m_window;

    URef<UserApplicationBase> m_app;
};