cbuffer PerDraw : register(b0)
{
    float4x4 viewProj;
    float4x4 model;
    float4x4 modelInvTranspose;
};

struct GBufferOutput
{
    float4 Albedo   : SV_Target0;
    float4 Normal   : SV_Target1;
    float4 Material : SV_Target2;
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 normal   : NORMAL;
    float2 uv0      : TEXCOORD0;
};

struct VSInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float4 tangent  : TANGENT;
    float2 uv0      : TEXCOORD0;
    float2 uv1      : TEXCOORD1;
    float4 color    : COLOR;
};

Texture2D    BaseColorTexture         : register(t0);
Texture2D    NormalTexture            : register(t1);
Texture2D    MetallicRoughnessTexture : register(t2);
Texture2D    OcclusionTexture         : register(t3);
Texture2D    EmissiveTexture          : register(t4);
SamplerState Sampler                  : register(s0);


VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.position = mul(viewProj, mul(model, float4(input.position, 1.0)));
    output.normal   = normalize(mul((float3x3)modelInvTranspose, input.normal));
    output.uv0      = input.uv0;
    return output;
}

GBufferOutput PSMain(VSOutput input)
{
    GBufferOutput output;
    output.Albedo = BaseColorTexture.Sample(Sampler, input.uv0);
    output.Normal = float4(normalize(input.normal), 0.0);
    output.Material = MetallicRoughnessTexture.Sample(Sampler, input.uv0);
    return output;
}