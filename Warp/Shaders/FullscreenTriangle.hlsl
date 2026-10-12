// Shared VS for full screen passes. Draw(3) with no vertex buffer.
//
// Vertices land at (-1,-1), (3,-1) and (-1,3). The clipper trims the parts
// outside the screen, so one triangle covers it exactly, with no seam down the
// diagonal like two triangles would have.
//
// A pass that only needs the pixel position can declare just SV_Position in its
// PS input, since it comes first here.

struct FullscreenVSOutput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0; // (0,0) is the top left
};

FullscreenVSOutput VSMain(uint vertexID : SV_VertexID)
{
    FullscreenVSOutput output;
    float2 position;
    position.x = (vertexID == 1) ? 3.0f : -1.0f;
    position.y = (vertexID == 2) ? 3.0f : -1.0f;
    output.position = float4(position, 0.0f, 1.0f);
    output.uv = position * float2(0.5, -0.5) + 0.5;

    return output;
}
