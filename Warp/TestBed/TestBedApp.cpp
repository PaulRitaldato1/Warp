#include <EntryPoint/EntryPoint.h>
#include <Common/CommonTypes.h>

struct TempGame : public UserApplicationBase
{
    bool Initialize()
    {
        return true;
    }

    bool Update(f32 DeltaTime)
    {
        if(DeltaTime > 0)
        {
        }
        // LOG_DEBUG("DeltaTime: " + std::to_string(DeltaTime));
        return true;
    }

    void OnResize(f32 DeltaTime)
    {
        if(DeltaTime > 1.0f)
        {
            // LOG_DEBUG("Whatever");
        }
    }
};

bool HookEngineFromApp(UserApplicationBase** outDesc)
{
    (*outDesc) = new TempGame();
    
    (*outDesc)->EngineInitDesc.Name = "Test Bed";
    (*outDesc)->EngineInitDesc.WindowWidth = 1920;
    (*outDesc)->EngineInitDesc.WindowHeight = 1080;

    return true;
}