#pragma once

class IWindow
{
public:

    virtual bool Create() = 0;

protected:

    bool m_bAppPaused = false;
    bool m_bMinimized = false;
    bool m_bMaximized = false;
    bool m_bResizing = false;
    bool m_bFullScreenState = false;

    int m_width;
    int m_height;
};