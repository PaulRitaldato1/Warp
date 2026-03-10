// [[vk::push_constant]] tells DXC/shaderc to emit this as a push-constant block
// instead of a uniform buffer. D3D12 ignores the attribute.
[[vk::push_constant]]
cbuffer PerDraw : register(b0)
{
    float4x4 viewProj;
    float4x4 model;
};

Texture2D    BaseColorTexture         : register(t0);
Texture2D    NormalTexture            : register(t1);
Texture2D    MetallicRoughnessTexture : register(t2);
Texture2D    OcclusionTexture         : register(t3);
Texture2D    EmissiveTexture          : register(t4);
SamplerState Sampler                  : register(s0);

struct VSInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float4 tangent  : TANGENT;
    float2 uv0      : TEXCOORD0;
    float2 uv1      : TEXCOORD1;
    float4 color    : COLOR;
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 normal   : NORMAL;
    float2 uv0      : TEXCOORD0;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.position = mul(viewProj, mul(model, float4(input.position, 1.0)));
    output.normal   = normalize(mul((float3x3)model, input.normal));
    output.uv0      = input.uv0;
    return output;
}

float4 PSMain(VSOutput input) : SV_Target
{
    return BaseColorTexture.Sample(Sampler, input.uv0);
}
