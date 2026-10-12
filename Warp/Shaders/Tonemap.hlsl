cbuffer TonemapConstants : register(b0)
{
    float exposureEV;
    float whitePoint;
    int tonemapper;
    float padding;
};

Texture2D HDRTarget : register(t0);

// The VS is FullscreenTriangle.hlsl. Only the pixel position is read, from its
// first output.
struct PSInput
{
    float4 position : SV_Position;
};

float3 LinearToSRGB(float3 color)
{
    color = max(color, 0.0f);
    float3 low = 12.92 * color;
    float3 high = 1.055 * pow(color, 1.0f/2.4f) - 0.055;
    return lerp(low, high, step(0.0031308, color));
}

float4 LinearToSRGB(float4 color)
{
    return float4(LinearToSRGB(color.rgb), color.a);
}

float3 Tonemap(float3 color)
{
    if(tonemapper == 0) //Clamp
    {
        return saturate(color);
    }
    else if(tonemapper == 1) //Reinhard
    {
        float whitePointGuard = max(whitePoint, 0.0001f);
        return color * (1.0 + color / (whitePointGuard * whitePointGuard)) / (1.0 + color);
    }
    else if(tonemapper == 2) //ACES, Narkowicz fit
    {
        const float a = 2.51;
        const float b = 0.03;
        const float c = 2.43;
        const float d = 0.59;
        const float e = 0.14;
        return saturate((color * (a * color + b)) / (color * (c * color + d) + e));
    }

    return saturate(color); //Clamp when unknown
}

float4 PSMain(PSInput input) : SV_Target
{
    float3 color = HDRTarget.Load(int3(input.position.xy, 0)).rgb;

    //Exposure
    color *= exp2(exposureEV);
    //Tonemap
    color = Tonemap(color.rgb);
    //Encode to SRGB
    color = LinearToSRGB(color.rgb);
    return  float4(color, 1.0f);
}
