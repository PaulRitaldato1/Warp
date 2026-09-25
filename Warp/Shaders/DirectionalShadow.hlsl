// Position only. The shadow pass binds just the position stream, so declaring
// anything else here would not match the pipeline's input layout.
struct VSInput
{
    float3 position : POSITION;
    uint instanceID : SV_InstanceID;
};

struct InstanceData
{
    float4x4 model;
    float4x4 modelInvTranspose;
    float3 boundsCenter;
    uint batchStart;
    float3 boundsExtents;
    uint batchInfo;
};

cbuffer PerView : register(b1)
{
    float4x4 lightViewProj;
};

cbuffer ShadowDrawConstants : register(b0)
{
    uint instanceOffset;
};

StructuredBuffer<InstanceData> instances : register(t0);

// Slots are allocated in arbitrary order, so a batch is a list of slot indices
// rather than a contiguous range.
StructuredBuffer<uint> instanceIndices : register(t1);

float4 VSMain(VSInput input) : SV_Position
{
    InstanceData inst = instances[instanceIndices[instanceOffset + input.instanceID]];
    return mul(lightViewProj, mul(inst.model, float4(input.position, 1.0)));
}
