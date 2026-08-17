// Position only. The shadow pass binds just the position stream, so declaring
// anything else here would not match the pipeline's input layout.
struct VSInput
{
    float3 position : POSITION;
};

cbuffer ShadowCB : register(b0)
{
    float4x4 lightViewProj;
    float4x4 model;
};

float4 VSMain(VSInput input) : SV_Position
{
    return mul(lightViewProj, mul(model, float4(input.position, 1.0)));
}