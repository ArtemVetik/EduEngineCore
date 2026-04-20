#include "gpu_noise_lib.hlsl"

#define PI 3.1415926536

SamplerState gsamLinearWrap : register(s2);

Texture2D gNoise : register(t0);
Texture2D gBlueNoise : register(t1);

cbuffer cbPass : register(b0)
{
    float gTime;
    uint gFrame;
    float2 gResolution;
    float3 gCameraPosition;
    uint gPadding0;
    float4x4 gLookAt;
    uint gPadding1;
}

cbuffer cbConstants : register(b1)
{
    float3 gSunPosition;
    uint gMaxSteps;
    float gMarchSize;
}

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

float BeersLaw(float dist, float absorption)
{
    return exp(-dist * absorption);
}

// Follows PBRT convention http://www.pbr-book.org/3ed-2018/Volume_Scattering/Phase_Functions.html#PhaseHG
float HenyeyGreensteinPhase(float G, float CosTheta)
{
	// Reference implementation (i.e. not schlick approximation). 
	// See http://www.pbr-book.org/3ed-2018/Volume_Scattering/Phase_Functions.html
    float Numer = 1.0f - G * G;
    float Denom = 1.0f + G * G + 2.0f * G * CosTheta;
    return Numer / (4.0f * PI * Denom * sqrt(Denom));
}

float noise(in float3 x)
{
    float3 p = floor(x);
    float3 f = frac(x);
    f = f * f * (3.0 - 2.0 * f);

    float2 uv = (p.xy + float2(37.0, -239.0) * p.z) + f.xy;
    float2 tex = gNoise.Sample(gsamLinearWrap, (uv + 0.5) / 256.0).yx;

    return lerp(tex.x, tex.y, f.z) * 2.0 - 1.0;
}

float fbm(float3 p, bool lowRes)
{
    float3 q = p + gTime * 0.5 * float3(0.0, 0.1, -0.2);
    float g = noise(q);

    float f = 0.0;
    float scale = 0.5;
    float factor = 2.02;

    int maxOctave = 6;

    if (lowRes)
    {
        maxOctave = 3;
    }
    
    for (int i = 0; i < maxOctave; i++)
    {
        f += scale * noise(q);
        q *= factor;
        factor += 0.21;
        scale *= 0.5;
    }

    return clamp(f - p.y, 0.0, 1.0);
}

float scene(float3 p, bool lowRes)
{
    return fbm(p, lowRes);
}

float4 raymarch(float3 rayOrigin, float3 rayDirection, float offset)
{
    float depth = gMarchSize * offset;
    float3 p = rayOrigin + depth * rayDirection;
    float3 sunDirection = normalize(gSunPosition);
    
    float4 color = 0.0;
    
    for (int i = 0; i < gMaxSteps; i++)
    {
        float density = scene(p, false);
        
        if (density > 0.0)
        {
            float4 c = float4(lerp(float3(1.0, 1.0, 1.0), float3(0.0, 0.0, 0.0), density), density);
            c.a *= 0.4;
            c.rgb *= c.a;
            color += c * (1.0 - color.a);
        }

        depth += gMarchSize;
        p = rayOrigin + depth * rayDirection;
    }
    
    return color;
}

float4 PS(VertexOut pIn) : SV_TARGET
{
    float2 uv = pIn.TexC;
    uv.y = 1 - uv.y;
    uv -= 0.5;
    uv.x *= gResolution.x / gResolution.y;
    
    float3 ro = gCameraPosition;
    float3 rd = mul(gLookAt, float4(normalize(float3(uv, 1.0)), 0));
    
    float3 color = 0.0;

    // Sun and Sky
    float3 sunColor = float3(1.0, 0.5, 0.3);
    float3 sunDirection = normalize(gSunPosition);
    float sun = clamp(dot(sunDirection, rd), 0.0, 1.0);
    // Base sky color
    color = float3(0.7, 0.7, 0.90);
    // Add vertical gradient
    color -= 0.8 * float3(0.90, 0.75, 0.90) * rd.y;

    // Cloud
    float blueNoise = gBlueNoise.Sample(gsamLinearWrap, pIn.TexC * gResolution / 1024.0).r;
    float offset = frac(blueNoise + float(gFrame % 32) / sqrt(0.5));
    
    float res = raymarch(ro, rd, offset);
    color = res;

    return float4(color, 1.0);
}
