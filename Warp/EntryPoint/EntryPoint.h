#pragma once

#include <Core/WarpEngine.h>
#include <Debugging/Assert.h>
#include <Debugging/Logging.h>
#include <UserApplication.h>

int main()
{
    UserApplicationBase* App = nullptr;

    FATAL_ASSERT(HookEngineFromApp(&App), "Failed to HookEngineFromApp");

    FATAL_ASSERT(App != nullptr, "App Failed to exist");

    WarpEngine Engine(App);
    Engine.Run();
}
