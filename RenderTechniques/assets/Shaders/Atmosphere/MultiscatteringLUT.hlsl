#include "AtmosphereData.hlsli"

Texture2D<float4> gTransmittanceLUT;
RWTexture2D<float4> gMultiscatteringLUT;

static const float PI = 3.14159265;
static const uint STEPS = 32;
static const uint SQRTSAMPLES = 8;

// Computes how far a ray can travel inside the atmosphere and
// whether it intersects the planet surface.
// Returns true if the ray hits the planet surface, false otherwise.
bool ComputeAtmosphereTraversalAndPlanetHit(float radius, float cosAngle, out float endDistance)
{
    float offset = -radius * cosAngle;
    float radius2 = radius * radius;
    float ray2Center2 = radius2 - offset * offset;
    float planetRadius2 = gPlanetRadius * gPlanetRadius;
    
    if (ray2Center2 < planetRadius2 && cosAngle < 0.0)
    {
        float bottomHalfLength = sqrt(planetRadius2 - ray2Center2);
        endDistance = offset - bottomHalfLength;
        return true;
    }
    else
    {
        float atmosphereRadius2 = gAtmosphereRadius * gAtmosphereRadius;
        float topHalfLength = sqrt(atmosphereRadius2 - ray2Center2);
        endDistance = topHalfLength + offset;
        return false;
    }
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

[numthreads(8, 8, 1)]
void ComputeMultiscatteringLUT(uint3 id : SV_DispatchThreadID)
{
    const float3 up = float3(0.0, 1.0, 0.0);
    const float uniformPhase = 1.0 / (4.0 * PI);

    float radius = lerp(0.0, gAtmosphereRadius - gPlanetRadius, (id.x + 0.5) / gLutWidth) + gPlanetRadius;
    float cosSunZenith = lerp(-1.0, 1.0, (id.y + 0.5) / gLutHeight);

    float3 rayOrigin = float3(0.0, radius, 0.0);
    float3 sunDirection = float3(sqrt(1.0 - cosSunZenith * cosSunZenith), cosSunZenith, 0.0);

    float3 luminance = 0.0;
    float3 transferFunction = 0.0;

    float3 rayleighExtinctionCoefficient = gRayleighScatteringCoefficient + gRayleighAbsorptionCoefficient;
    float3 mieExtinctionCoefficient = gMieScatteringCoefficient + gMieAbsorptionCoefficient;
    float3 ozoneExtinctionCoefficient = gOzoneScatteringCoefficient + gOzoneAbsorptionCoefficient;
    
    const uint SAMPLES = SQRTSAMPLES * SQRTSAMPLES;
    for (uint i = 0; i < SAMPLES; i++)
    {
        float z = (float(i) + 0.5) / float(SAMPLES);
        float xy = sqrt(1.0 - z * z);
        float azimuth = z * float(SQRTSAMPLES) * PI * 2.0;
        float3 rayDirection = float3(sin(azimuth) * xy, cos(azimuth) * xy, z);

        float endDistance;
        bool hitGround = ComputeAtmosphereTraversalAndPlanetHit(radius, rayDirection.y, endDistance);
        float stepSize = endDistance / float(STEPS);
        
        float3 segmentLuminance = 0.0;
        float3 segmentTransferFunction = 0.0;
        float3 transmittance = 1.0;
        float currentDistance = stepSize * 0.5;
        
        for (uint j = 0; j < STEPS; j++)
        {
            float3 samplePosition = rayOrigin + currentDistance * rayDirection;
            float sampleRadius = length(samplePosition);
            float sampleHeight = sampleRadius - gPlanetRadius;

            float rayleighDensity = exp(-sampleHeight / gRayleighScaleHeight);
            float mieDensity = exp(-sampleHeight / gMieScaleHeight);
            float ozoneDensity = max(0.0, 1.0 - (sampleHeight - 25000) / 15000);

            float cosSunAngle = dot(up, sunDirection);
            float3 transmittanceToSun = SampleTransmittanceLUT(sampleRadius, cosSunAngle);
            float3 sampleScattering = gRayleighScatteringCoefficient * rayleighDensity + gMieScatteringCoefficient * mieDensity + gOzoneScatteringCoefficient * ozoneDensity;
            float3 inscattering = transmittanceToSun * sampleScattering * uniformPhase;

            float3 sampleExtinction = rayleighExtinctionCoefficient * rayleighDensity + mieExtinctionCoefficient * mieDensity + ozoneExtinctionCoefficient * ozoneDensity;
            float3 sampleTransmittance = exp(-sampleExtinction * stepSize);
            float3 nextTransmittance = transmittance * sampleTransmittance;
            float3 transmittanceIntegral = (transmittance - nextTransmittance) / sampleExtinction;

            segmentLuminance += transmittanceIntegral * inscattering;
            segmentTransferFunction += transmittanceIntegral * sampleScattering;

            currentDistance += stepSize;
            transmittance = nextTransmittance;
        }

        luminance += segmentLuminance;
        transferFunction += segmentTransferFunction;
        
        if (hitGround)
        {
            luminance += transmittance * SampleTransmittanceLUT(radius, cosSunZenith) * (gGroundSpectrumAlbedo / PI) * cosSunZenith;
        }
    }

    float3 color = luminance / (float(SAMPLES) - transferFunction);

    gMultiscatteringLUT[id.xy] = float4(color, 1.0);
}