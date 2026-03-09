float4 VSMain(uint vid : SV_VertexID) : SV_Position
{
    float2 pos[3] = {
        float2( 0.0,  0.5),
        float2( 0.5, -0.5),
        float2(-0.5, -0.5)
    };
    return float4(pos[vid], 0.0, 1.0);
}

float4 PSMain() : SV_Target
{
    return float4(1.0, 0.5, 0.0, 1.0);
}
