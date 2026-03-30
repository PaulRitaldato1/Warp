cbuffer PerDraw : register(b0)
{
    float4x4 viewProj;
    float4x4 model;
    float4x4 modelInvTranspose;
    float3   emissiveFactor;
    float    cbPadding;
};

struct GBufferOutput
{
    float4 Albedo   : SV_Target0;
    float4 Normal   : SV_Target1;
    float4 Material : SV_Target2;
    float4 Emissive : SV_Target3;
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 normal   : NORMAL;
    float3 tangent  : TANGENT;
    float  bitangentSign : TEXCOORD1;
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
    output.position     = mul(viewProj, mul(model, float4(input.position, 1.0)));
    output.normal       = normalize(mul((float3x3)modelInvTranspose, input.normal));
    output.tangent      = normalize(mul((float3x3)modelInvTranspose, input.tangent.xyz));
    output.bitangentSign = input.tangent.w;
    output.uv0          = input.uv0;
    return output;
}

GBufferOutput PSMain(VSOutput input)
{
    GBufferOutput output;
    output.Albedo = BaseColorTexture.Sample(Sampler, input.uv0);

    // Tangent-space normal mapping.
    float3 normal    = normalize(input.normal);
    float3 tangent   = normalize(input.tangent);
    tangent          = normalize(tangent - normal * dot(tangent, normal)); // re-orthogonalize
    float3 bitangent = cross(normal, tangent) * input.bitangentSign;

    float3 texNormal  = NormalTexture.Sample(Sampler, input.uv0).rgb;
    texNormal         = texNormal * 2.0 - 1.0; // [0,1] -> [-1,1]
    float3 worldNormal = normalize(texNormal.x * tangent + texNormal.y * bitangent + texNormal.z * normal);

    float4 materialSample = MetallicRoughnessTexture.Sample(Sampler, input.uv0);
    float  occlusion      = OcclusionTexture.Sample(Sampler, input.uv0).r; // sampled to keep t3 register alive

    output.Normal   = float4(worldNormal, materialSample.b); // rgb = normal, a = metallic
    output.Material = float4(occlusion, materialSample.g, materialSample.b, 1.0); // r = occlusion (unused by lighting for now), g = roughness, b = metallic
    output.Emissive = float4(EmissiveTexture.Sample(Sampler, input.uv0).rgb * emissiveFactor, 1.0);
    return output;
}