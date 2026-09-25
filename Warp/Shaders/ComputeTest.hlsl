// Throwaway Phase 2 check. Writes a known pattern so the CPU can read it back
// and prove compute pipelines, root UAVs and readback all work.

cbuffer TestConstants : register(b0)
{
    uint count;
    uint multiplier;
};

RWStructuredBuffer<uint> output : register(u0);

[numthreads(64, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    // The dispatch rounds up to a whole group, so the last one overhangs.
    if (id.x >= count)
    {
        return;
    }

    output[id.x] = id.x * multiplier;
}
