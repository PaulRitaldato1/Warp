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
    float pad0;
    float3 boundsExtents;
    float pad1;
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

float4 VSMain(VSInput input) : SV_Position
{
    return mul(lightViewProj, mul(instances[instanceOffset + input.instanceID].model, float4(input.position, 1.0)));
}
