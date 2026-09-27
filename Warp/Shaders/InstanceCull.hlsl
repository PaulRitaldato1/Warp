// Frustum culls every instance slot and appends survivors to their batches'
// regions of the visible index list. Each batch's instanceCount doubles as its
// append counter, so the draw that follows reads exactly the survivors.

struct InstanceData
{
    float4x4 model;
    float4x4 modelInvTranspose;
    float3 boundsCenter;
    uint batchStart;
    float3 boundsExtents;
    uint batchInfo; // batch count low 16 bits, render flags high 16
};

// Matches DrawIndexedArgs in CommandList.h.
struct DrawArgs
{
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int baseVertex;
    uint firstInstance;
};

cbuffer CullConstants : register(b0)
{
    float4 planes[6];     // left, right, bottom, top, near, far
    uint slotCount;
    uint requiredFlags;
};

StructuredBuffer<InstanceData> instances  : register(t0);
StructuredBuffer<uint>         regionStarts : register(t1);

RWStructuredBuffer<DrawArgs> drawArgs       : register(u0);
RWStructuredBuffer<uint>     visibleIndices : register(u1);

// 64 is a multiple of both AMD's wave and NVIDIA's warp.
[numthreads(64, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    const uint slot = id.x;

    // The dispatch rounds up to a whole group, so the last one overhangs.
    if (slot >= slotCount)
    {
        return;
    }

    const InstanceData inst = instances[slot];

    // Free slots carry no flags, so they fail here too.
    const uint flags = inst.batchInfo >> 16;
    if ((flags & requiredFlags) != requiredFlags)
    {
        return;
    }

    // Same test as IsVisible in Frustum.h.
    [unroll]
    for (uint p = 0; p < 6; ++p)
    {
        const float distance = dot(planes[p].xyz, inst.boundsCenter) + planes[p].w;
        const float radius   = dot(abs(planes[p].xyz), inst.boundsExtents);
        if (distance < -radius)
        {
            return;
        }
    }

    // Tested once, emitted into every submesh's batch. InterlockedAdd returns the
    // count before the add, which is this instance's position in the region.
    const uint batchCount = inst.batchInfo & 0xFFFF;
    for (uint b = 0; b < batchCount; ++b)
    {
        const uint batch = inst.batchStart + b;

        uint local;
        InterlockedAdd(drawArgs[batch].instanceCount, 1, local);
        visibleIndices[regionStarts[batch] + local] = slot;
    }
}
