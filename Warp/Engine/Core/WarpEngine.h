#pragma once

#include <Common/CommonTypes.h>
#include <Core/GameTimer.h>
#include <Input/Input.h>
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
    ~WarpEngine();
    bool Run();

private:

    WarpEngine(WarpEngine const&) = delete;
    void operator=(WarpEngine const&) = delete;


    bool m_bIsRunning;
    bool m_bIsSuspended;
    bool m_bIsFullScreen;
    bool m_bResizing;

    f64 m_lastTime;
    GameTimer m_timer;

    // URef<IRenderer> m_renderer;
    URef<IWindow> m_window;

    URef<UserApplicationBase> m_app;
};
