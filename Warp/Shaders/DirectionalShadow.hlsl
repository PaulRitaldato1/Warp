struct VSInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float4 tangent  : TANGENT;
    float2 uv0      : TEXCOORD0;
    float2 uv1      : TEXCOORD1;
    float4 color    : COLOR;
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