#pragma once

#include <Common/CommonTypes.h>
#include <Core/WarpEngine.h>
#include <Input/Input.h>

//The user of the engine MUST implement this
extern bool HookEngineFromApp(UserApplicationBase** outDesc);

struct WARP_API UserApplicationBase
{
    EngineDesc EngineInitDesc;

    virtual ~UserApplicationBase() = default;

    virtual bool Initialize() = 0;

    virtual bool Update(f32 DeltaTime) = 0;

    virtual void OnResize(f32 DeltaTime) = 0;
};
