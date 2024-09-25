#pragma once

#include <Core/WarpEngine.h>
#include <Debugging/Logging.h>

struct Application
{
    bool (*Initialize)(struct Application* App);

    bool (*Update)(struct Application* App, f32 DeltaTime);

    void (*OnResize)(struct Application* App, f32 DeltaTime);
};

//The user of the engine MUST implement this and pass in the required function pointers
extern bool HookEngineFromApp(Application* outDesc);

int main()
{

}