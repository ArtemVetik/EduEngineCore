#include "AtmosphereData.hlsli"

static const float PI = 3.14159265;
static const uint STEPS = 500;

RWTexture2D<float4> gTransmittanceLUT;

// https://sebh.github.io/publications/egsr2020.pdf (formula 2)
[numthreads(8, 8, 1)]
void ComputeTransmittanceLUT(uint3 id : SV_DispatchThreadID)
{
    float radius = lerp(0.0, gAtmosphereRadius - gPlanetRadius, (id.x + 0.5) / gLutWidth) + gPlanetRadius;
    float cosSunZenith = lerp(-1.0, 1.0, (id.y + 0.5) / gLutHeight);
    
    float3 rayleighExtinctionCoefficient = gRayleighScatteringCoefficient + gRayleighAbsorptionCoefficient;
    float3 mieExtinctionCoefficient = gMieScatteringCoefficient + gMieAbsorptionCoefficient;
    float3 ozoneExtinctionCoefficient = gOzoneScatteringCoefficient + gOzoneAbsorptionCoefficient;

    float discriminant = max(0.0, radius * radius * (cosSunZenith * cosSunZenith - 1.0) + gAtmosphereRadius * gAtmosphereRadius);
    float stepSize = max(0.0, -radius * cosSunZenith + sqrt(discriminant)) / float(STEPS);

    float3 extinction = 0.0;
    
    for (uint i = 0; i < STEPS; i++)
    {
        float sampleDistance = (float(i) + 0.5) * stepSize;
        float sampleRadius = sqrt(sampleDistance * sampleDistance + 2.0 * radius * cosSunZenith * sampleDistance + radius * radius);
        float sampleHeight = sampleRadius - gPlanetRadius;

        float rayleighDensity = exp(-sampleHeight / gRayleighScaleHeight);
        float mieDensity = exp(-sampleHeight / gMieScaleHeight);
        float ozoneDensity = max(0.0, 1.0 - (sampleHeight - 25000) / 15000);

        extinction += (rayleighExtinctionCoefficient * rayleighDensity + mieExtinctionCoefficient * mieDensity + ozoneExtinctionCoefficient * ozoneDensity) * stepSize;
    }
    
    gTransmittanceLUT[id.xy] = float4(exp(-extinction), 0.0);
}