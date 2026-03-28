static const float PI = 3.14159265359;

cbuffer LightPassConstants : register(b0)
{
    float4x4 invViewProj;
    float3   cameraPosition;
    int      lightCount;

    // Sky parameters
    float3 sunDirection;
    float  sunDiscSize;
    float3 skyColorZenith;
    float  sunIntensity;
    float3 skyColorHorizon;
    float  horizonSharpness;
    float3 groundColor;
    float  groundFade;
    float3 sunColor;
    int    skyEnabled;
};

struct LightInfo
{
    float3 position;
    float  range;
    float3 color;
    float  intensity;
    float3 direction;
    int    type; // 0 = point, 1 = directional, 2 = spotlight
    float  innerConeAngle;
    float  outerConeAngle;
    float2 padding;
};

StructuredBuffer<LightInfo> LightBuffer : register(t4);

Texture2D        GBufferAlbedo   : register(t0);
Texture2D        GBufferNormal   : register(t1);
Texture2D        GBufferMaterial : register(t2);
Texture2D<float> GBufferDepth    : register(t3);
SamplerState     PointSampler    : register(s0);

struct VSOutput
{
    float4 position     : SV_Position;
    float2 uv           : TEXCOORD0;
    float3 worldFarPos  : TEXCOORD1;
};

VSOutput VSMain(uint vertexID : SV_VertexID)
{
    VSOutput output;
    float2 position;
    position.x = (vertexID == 1) ? 3.0f : -1.0f;
    position.y = (vertexID == 2) ? 3.0f : -1.0f;
    output.position = float4(position, 0.0f, 1.0f);
    output.uv = position * float2(0.5, -0.5) + 0.5;

    // Project the corner onto the far plane so the PS can get view direction cheaply.
    float4 clipFar = float4(position, 1.0, 1.0);
    float4 worldFar = mul(invViewProj, clipFar);
    output.worldFarPos = worldFar.xyz / worldFar.w;

    return output;
}

// GGX Normal Distribution Function.
// Models what fraction of microfacets are oriented exactly toward the halfway vector.
// A perfectly smooth surface (roughness=0) produces a perfect mirror — all microfacets
// face the same direction. A rough surface scatters them, giving a wider highlight.
float DistributionGGX(float normalDotHalfway, float roughness)
{
    float roughnessSq        = roughness * roughness;
    float roughnessSq2       = roughnessSq * roughnessSq; // GGX uses alpha^2
    float denom              = (normalDotHalfway * normalDotHalfway) * (roughnessSq2 - 1.0) + 1.0;
    return roughnessSq2 / (PI * denom * denom);
}

// Fresnel-Schlick approximation.
// Describes how much light is reflected vs absorbed depending on the viewing angle.
// At grazing angles (viewDotHalfway near 0) almost all light is reflected regardless of material.
// baseReflectivity (F0) is 0.04 for non-metals and the albedo color for metals.
float3 FresnelSchlick(float viewDotHalfway, float3 baseReflectivity)
{
    return baseReflectivity + (1.0 - baseReflectivity) * pow(saturate(1.0 - viewDotHalfway), 5.0);
}

// Smith GGX Geometry Function.
// Models microfacet self-shadowing: at steep angles microfacets can block each other
// from the light or the camera, reducing the effective contribution of the surface.
// Applied from both the light direction and the view direction independently, then combined.
float GeometrySmith(float normalDotView, float normalDotLight, float roughness)
{
    float k                    = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    float viewShadowing        = normalDotView  / (normalDotView  * (1.0 - k) + k);
    float lightShadowing       = normalDotLight / (normalDotLight * (1.0 - k) + k);
    return viewShadowing * lightShadowing;
}

float4 PSMain(VSOutput input) : SV_Target
{
    float depth = GBufferDepth.Sample(PointSampler, input.uv);

    // Nothing was drawn to this pixel — render procedural sky if a SunComponent exists.
    if (depth >= 1.0)
    {
        if (!skyEnabled)
        {
            return float4(0.0, 0.0, 0.0, 1.0);
        }
        // View direction from interpolated far-plane position (computed in VS).
        float3 viewDir = normalize(input.worldFarPos - cameraPosition);

        // Vertical gradient: blend horizon -> zenith based on how far up we look.
        float upAmount      = viewDir.y;
        float horizonBlend  = 1.0 - pow(saturate(upAmount), horizonSharpness);
        float3 skyColor     = lerp(skyColorZenith, skyColorHorizon, horizonBlend);

        // Below the horizon: blend toward ground color.
        float belowHorizon  = saturate(-upAmount * groundFade);
        skyColor = lerp(skyColor, groundColor, belowHorizon);

        // Sun disc: bright spot where view direction aligns with sun direction.
        float sunDot        = dot(viewDir, sunDirection);
        float sunMask       = smoothstep(sunDiscSize, sunDiscSize + 0.0005, sunDot);
        skyColor += sunColor * sunIntensity * sunMask;

        // Subtle sun glow around the disc.
        float sunGlow       = pow(saturate(sunDot), 128.0);
        skyColor += sunColor * sunGlow * 0.3;

        // Gamma correction.
        skyColor = pow(max(skyColor, 0.0), 1.0 / 2.2);
        return float4(skyColor, 1.0);
    }

    // Reconstruct world-space position from depth.
    float2 ndcXY        = input.uv * float2(2.0, -2.0) + float2(-1.0, 1.0);
    float4 clipSpacePos = float4(ndcXY, depth, 1.0);
    float4 worldSpacePos = mul(invViewProj, clipSpacePos);
    worldSpacePos.xyz   /= worldSpacePos.w; // Perspective divide

    // Sample GBuffer.
    float4 albedoSample   = GBufferAlbedo.Sample(PointSampler, input.uv);
    float4 normalSample   = GBufferNormal.Sample(PointSampler, input.uv);
    float4 materialSample = GBufferMaterial.Sample(PointSampler, input.uv);

    // Unpack GBuffer values.
    // Normal RT: rgb = world-space normal, a = metallic
    // Material RT: g = roughness, b = metallic (glTF convention)
    float3 albedo    = albedoSample.rgb;
    float3 normal    = normalize(normalSample.rgb);
    float  metallic  = normalSample.a;
    float  roughness = max(materialSample.g, 0.05); // Clamp to avoid division by zero in BRDF

    float3 viewDir = normalize(cameraPosition - worldSpacePos.xyz);

    // Base reflectivity at normal incidence.
    // Non-metals reflect about 4% of light regardless of color (0.04).
    // Metals use their albedo as the tint of their reflections.
    float3 baseReflectivity = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);

    float3 totalRadiance = float3(0.0, 0.0, 0.0);

    for (int i = 0; i < lightCount; ++i)
    {
        LightInfo light = LightBuffer[i];

        float3 lightDir;
        float  attenuation = 1.0;

        if (light.type == 1) // Directional — light comes from an infinite distance, no attenuation.
        {
            lightDir = normalize(-light.direction);
        }
        else // Point or Spot — light radiates outward from a position, falls off with distance.
        {
            float3 toLight = light.position - worldSpacePos.xyz;
            float  dist    = length(toLight);
            lightDir       = normalize(toLight);

            // Range-normalized inverse-square with smooth window.
            // Dividing by (d/r)^2 instead of d^2 means doubling range doubles the
            // visible reach rather than just moving a cutoff nobody can see.
            // The (1-(d/r)^4)^2 window ensures a clean fade to zero at range.
            float distOverRange  = dist / light.range;
            float distOverRange2 = distOverRange * distOverRange;
            float distOverRange4 = distOverRange2 * distOverRange2;
            float rangeWindow    = saturate(1.0 - distOverRange4);
            attenuation = (rangeWindow * rangeWindow) / (distOverRange2 + 0.01);

            if (light.type == 2) // Spot — cone falloff on top of point attenuation.
            {
                float cosAngle      = dot(normalize(light.direction), -lightDir);
                float cosInner      = cos(radians(light.innerConeAngle));
                float cosOuter      = cos(radians(light.outerConeAngle));
                float spotFalloff   = saturate((cosAngle - cosOuter) / (cosInner - cosOuter));
                attenuation        *= spotFalloff * spotFalloff;
            }
        }

        float normalDotLight = max(dot(normal, lightDir), 0.0);

        // Light contributes nothing if it hits the back of the surface.
        if (normalDotLight <= 0.0)
        {
            continue;
        }

        // Halfway vector: bisects the angle between the view and light directions.
        // The BRDF is evaluated at this vector because it represents the microfacet
        // orientation that would reflect light exactly toward the camera.
        float3 halfwayDir = normalize(viewDir + lightDir);

        float normalDotView     = max(dot(normal,     viewDir),    0.0);
        float normalDotHalfway  = max(dot(normal,     halfwayDir), 0.0);
        float viewDotHalfway    = max(dot(viewDir,    halfwayDir), 0.0);

        // Cook-Torrance BRDF specular term: D*F*G / (4 * NdotV * NdotL)
        // D: how many microfacets face the halfway direction
        // F: how much light is reflected (vs absorbed) at this angle
        // G: how much microfacet self-shadowing reduces the contribution
        // The 4*NdotV*NdotL denominator is a normalization factor for the geometry
        float  microfacetDistribution = DistributionGGX(normalDotHalfway, roughness);
        float3 fresnelReflectance     = FresnelSchlick(viewDotHalfway, baseReflectivity);
        float  geometryShadowing      = GeometrySmith(normalDotView, normalDotLight, roughness);

        float3 specular = (microfacetDistribution * fresnelReflectance * geometryShadowing)
                        / max(4.0 * normalDotView * normalDotLight, 0.001);

        // Fresnel gives the specular fraction. The remainder goes to diffuse.
        // Metals have no diffuse term — they absorb refracted light instead of scattering it.
        float3 specularFraction = fresnelReflectance;
        float3 diffuseFraction  = (1.0 - specularFraction) * (1.0 - metallic);
        float3 diffuse          = diffuseFraction * albedo / PI;

        float3 incomingRadiance = light.color * light.intensity * attenuation;
        totalRadiance += (diffuse + specular) * incomingRadiance * normalDotLight;
    }

    float3 ambient = float3(0.03, 0.03, 0.03) * albedo;
    float3 color   = ambient + totalRadiance;

    // Gamma correction (linear -> sRGB).
    color = pow(max(color, 0.0), 1.0 / 2.2);

    return float4(color, 1.0);
}
