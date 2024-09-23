#pragma once

#include <Common/CommonTypes.h>

class IWindow
{
public:

    virtual ~IWindow() {}

    virtual bool Create(String AppName, int width, int height) = 0;
    virtual void Destroy() = 0;

protected:

    bool m_bAppPaused = false;
    bool m_bMinimized = false;
    bool m_bMaximized = false;
    bool m_bResizing = false;
    bool m_bFullScreenState = false;

    int m_width;
    int m_height;
};