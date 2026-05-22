#include "AtmosphereData.hlsli"

static const float PI = 3.14159265;
static const uint STEPS = 32;

Texture2D<float4> gTransmittanceLUT;
Texture2D<float4> gMultiscatteringLUT;
RWTexture2D<float4> gSkyViewLUT;

// Computes the segment of a ray that travels through the atmosphere.
// Returns true if the ray hits the planet surface, false otherwise.
bool ComputeAtmosphereTraversalAndPlanetHit(float radius, float cosAngle, out float startDistance, out float endDistance)
{
    float offset = -radius * cosAngle;
    float radius2 = radius * radius;
    float ray2Center2 = radius2 - offset * offset;
    float planetRadius2 = gPlanetRadius * gPlanetRadius;
    float atmosphereRadius2 = gAtmosphereRadius * gAtmosphereRadius;
    float topHalfLength = sqrt(atmosphereRadius2 - ray2Center2);
    startDistance = max(0.0, offset - topHalfLength);
    
    if (ray2Center2 < planetRadius2 && cosAngle < 0.0)
    {
        float bottomHalfLength = sqrt(planetRadius2 - ray2Center2);
        endDistance = offset - bottomHalfLength;
        return true;
    }
    else
    {
        endDistance = topHalfLength + offset;
        return false;
    }
}

// Computes the Mie phase function.
// https://sebh.github.io/publications/egsr2020.pdf (Section 4)
float GetMiePhase(float cosTheta)
{
    float scale = 3.0 / (8.0 * PI);
    
    float num = (1.0 - gMieG * gMieG) * (1.0 + cosTheta * cosTheta);
    float denom = (2.0 + gMieG * gMieG) * pow(abs(1.0 + gMieG * gMieG - 2.0 * gMieG * cosTheta), 1.5);
    
    return scale * num / denom;
}

// Computes the Rayleigh phase function.
// https://sebh.github.io/publications/egsr2020.pdf (Section 4)
float GetRayleighPhase(float cosTheta)
{
    float scale = 3.0 / (16.0 * PI);

    return scale * (1.0 + cosTheta * cosTheta);
}

// Samples transmittance LUT.
// https://sebh.github.io/publications/egsr2020.pdf (Section 5.5.2)
float3 SampleTransmittanceLUT(float radius, float cosAngle)
{
    float2 uv = float2(
        clamp((radius - gPlanetRadius) / (gAtmosphereRadius - gPlanetRadius), 0.0, 1.0),
        clamp(0.5 + 0.5 * cosAngle, 0.0, 1.0)
    );
    return gTransmittanceLUT.SampleLevel(gsamAnisotropicClamp, uv, 0).rgb;
}

// Samples multiscattering LUT.
// https://sebh.github.io/publications/egsr2020.pdf (Section 5.5.2)
float3 SampleMultiscatteringLUT(float radius, float cosAngle)
{
    float2 uv = float2(
        clamp((radius - gPlanetRadius) / (gAtmosphereRadius - gPlanetRadius), 0.0, 1.0),
        clamp(0.5 + 0.5 * cosAngle, 0.0, 1.0)
    );
    return gMultiscatteringLUT.SampleLevel(gsamAnisotropicClamp, uv, 0).rgb;
}

[numthreads(8, 8, 1)]
void ComputeSkyViewLUT(uint3 id : SV_DispatchThreadID)
{
    float longitude = lerp(-PI, PI, (id.x + 0.5) / (gLutWidth - 1.0));
    float v = lerp(1.0, 0.0, (id.y + 0.5) / (gLutHeight - 1.0));

    float radius = gPlanetRadius;
    float horizon = sqrt(radius * radius - gPlanetRadius * gPlanetRadius);
    float beta = acos(horizon / radius);
    float zenithHorizonAngle = PI - beta;

    float latitude = v * 2.0 - 1.0;
    latitude *= latitude;
    
#if 0
    if (v < 0.5)
        latitude = (1.0 - latitude) * zenithHorizonAngle;
    else
        latitude = zenithHorizonAngle + latitude * beta;
#else    
    latitude = (1.0 - latitude) * zenithHorizonAngle;
#endif
    
    float3 rayDirection = float3(sin(longitude) * sin(latitude), cos(latitude), cos(longitude) * sin(latitude));
    float3 rayOrigin = float3(0.0, radius, 0.0);
    float3 sunDirection = normalize(gSunDirection);

    float cosSunRay = dot(rayDirection, sunDirection);

    float3 rayleighPhase = gRayleighScatteringCoefficient * GetRayleighPhase(cosSunRay);
    float3 miePhase = gMieScatteringCoefficient * GetMiePhase(cosSunRay);

    float startDistance, endDistance;
    ComputeAtmosphereTraversalAndPlanetHit(radius, cos(latitude), startDistance, endDistance);
    float stepSize = (endDistance - startDistance) / float(STEPS);

    float3 transmittance = 1.0;
    float3 luminance = 0.0;
    float currentDistance = stepSize * 0.5 + startDistance;

    float3 rayleighExtinctionCoefficient = gRayleighScatteringCoefficient + gRayleighAbsorptionCoefficient;
    float3 mieExtinctionCoefficient = gMieScatteringCoefficient + gMieAbsorptionCoefficient;
    float3 ozoneExtinctionCoefficient = gOzoneScatteringCoefficient + gOzoneAbsorptionCoefficient;

    for (uint i = 0; i < STEPS; i++)
    {
        float3 samplePosition = rayOrigin + currentDistance * rayDirection;
        float sampleRadius = length(samplePosition);
        float sampleHeight = sampleRadius - gPlanetRadius;

        float rayleighDensity = exp(-sampleHeight / gRayleighScaleHeight);
        float mieDensity = exp(-sampleHeight / gMieScaleHeight);
        float ozoneDensity = max(0.0, 1.0 - (sampleHeight - 25000) / 15000);

        float cosSunAngle = dot(normalize(samplePosition), sunDirection);
        float3 transmittanceToSun = SampleTransmittanceLUT(sampleRadius, cosSunAngle);
        float3 inscattering = transmittanceToSun * (rayleighDensity * rayleighPhase + mieDensity * miePhase);
        float3 sampleScattering = gRayleighScatteringCoefficient * rayleighDensity + gMieScatteringCoefficient * mieDensity + gOzoneScatteringCoefficient * ozoneDensity;
        inscattering += SampleMultiscatteringLUT(sampleRadius, cosSunAngle) * sampleScattering;

        float3 sampleExtinction = rayleighExtinctionCoefficient * rayleighDensity + mieExtinctionCoefficient * mieDensity + ozoneExtinctionCoefficient * ozoneDensity;
        float3 sampleTransmittance = exp(-sampleExtinction * stepSize);
        float3 nextTransmittance = transmittance * sampleTransmittance;
        float3 transmittanceIntegral = (transmittance - nextTransmittance) / sampleExtinction;

        luminance += transmittanceIntegral * inscattering;
        currentDistance += stepSize;
        transmittance = nextTransmittance;
    }
    
    gSkyViewLUT[id.xy] = float4(luminance, 1.0);
}