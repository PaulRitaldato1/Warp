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
    float4 color    : COLOR;
    float3 normal   : NORMAL;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.position = float4(input.position, 1.0);
    output.color    = input.color;
    output.normal   = input.normal * 0.5 + 0.5; // visualise normals as colour
    return output;
}

float4 PSMain(VSOutput input) : SV_Target
{
    return float4(input.normal, 1.0);
}
