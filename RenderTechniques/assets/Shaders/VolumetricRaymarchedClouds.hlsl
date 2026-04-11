SamplerState gsamLinearWrap : register(s2);

Texture2D gNoise : register(t0);
Texture2D gBlueNoise : register(t1);

cbuffer cbPass : register(b0)
{
    float gTime;
    uint gFrame;
    float2 gResolution;
}

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

#define PI 3.1415926536
#define MAX_STEPS 50
#define MAX_STEPS_LIGHTS 6
#define ABSORPTION_COEFFICIENT 0.9
#define SCATTERING_ANISO 0.3

float sdSphere(float3 p, float radius)
{
    return length(p) - radius;
}

float BeersLaw(float dist, float absorption)
{
    return exp(-dist * absorption);
}

float HenyeyGreenstein(float g, float mu)
{
    float gg = g * g;
    return (1.0 / (4.0 * PI)) * ((1.0 - gg) / pow(1.0 + gg - 2.0 * g * mu, 1.5));
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
    float3 q = p + gTime * 0.5 * float3(1.0, -0.2, -1.0);
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

    return f;
}

float scene(float3 p, bool lowRes)
{
    float distance = sdSphere(p, 1.0);
    float f = fbm(p, lowRes);

    return -distance + f;
}

static const float3 SUN_POSITION = float3(1.0, 0.0, 0.0);
static const float MARCH_SIZE = 0.16;

float lightmarch(float3 position, float3 rayDirection)
{
    float3 lightDirection = normalize(SUN_POSITION);
    float totalDensity = 0.0;
    float marchSize = 0.03;
 
    for (int step = 0; step < MAX_STEPS_LIGHTS; step++)
    {
        position += lightDirection * marchSize * float(step);
            
        float lightSample = scene(position, true);
        totalDensity += lightSample;
    }

    float transmittance = BeersLaw(totalDensity, ABSORPTION_COEFFICIENT);
    return transmittance;
}

float raymarch(float3 rayOrigin, float3 rayDirection, float offset)
{
    float depth = MARCH_SIZE * offset;
    float3 p = rayOrigin + depth * rayDirection;
    float3 sunDirection = normalize(SUN_POSITION);
    
    float totalTransmittance = 1.0;
    float lightEnergy = 0.0;
    
    float phase = HenyeyGreenstein(SCATTERING_ANISO, dot(rayDirection, sunDirection));
    
    for (int i = 0; i < MAX_STEPS; i++)
    {
        float density = scene(p, false);

        // We only draw the density if it's greater than 0
        if (density > 0.0)
        {
            float lightTransmittance = lightmarch(p, rayDirection);
            float luminance = 0.025 + density * phase;

            totalTransmittance *= lightTransmittance;
            lightEnergy += totalTransmittance * luminance;
        }

        depth += MARCH_SIZE;
        p = rayOrigin + depth * rayDirection;
    }

    return lightEnergy;
}

float4 PS(VertexOut pIn) : SV_TARGET
{
    float2 uv = pIn.TexC;
    uv -= 0.5;
    uv.x *= gResolution.x / gResolution.y;

    // Ray Origin - camera
    float3 ro = float3(0.0, 0.0, 5.0);
    // Ray Direction
    float3 rd = normalize(float3(uv, -1.0));
  
    float3 color = 0.0;

    // Sun and Sky
    float3 sunColor = float3(1.0, 0.5, 0.3);
    float3 sunDirection = normalize(SUN_POSITION);
    float sun = clamp(dot(sunDirection, rd), 0.0, 1.0);
    // Base sky color
    color = float3(0.7, 0.7, 0.90);
    // Add vertical gradient
    color -= 0.8 * float3(0.90, 0.75, 0.90) * rd.y;

    // Cloud
    float blueNoise = gBlueNoise.Sample(gsamLinearWrap, pIn.TexC * gResolution / 1024.0).r;
    float offset = frac(blueNoise + float(gFrame % 32) / sqrt(0.5));
    
    float res = raymarch(ro, rd, offset);
    color = color + sunColor * res;

    return float4(color, 1.0);
}
