struct ReflectionProbe
{
    float3 Position;
    float3 BoxExtents;
    uint IrradianceMapIdx;
    uint PrefilteredMapIdx;
};

float VolumeDistanceToReflectionProbe(float3 posW, ReflectionProbe p)
{
    float3 local = posW - p.Position;
    float3 d = abs(local) / p.BoxExtents;
    return max(d.x, max(d.y, d.z));
}

float3 SampleProbeIrradiance(ReflectionProbe probe, SamplerState sam, float3 N)
{
    TextureCube irradianceMapTex = ResourceDescriptorHeap[probe.IrradianceMapIdx];
    return irradianceMapTex.Sample(sam, N).rgb;
}

float3 SampleProbePrefilteredColor(ReflectionProbe probe, SamplerState sam, float3 R, float prefilteredLod)
{
    TextureCube prefilteredMapTex = ResourceDescriptorHeap[probe.PrefilteredMapIdx];
    return prefilteredMapTex.SampleLevel(sam, R, prefilteredLod).rgb;
}

void CalculateProbesIrradianceAndReflection(StructuredBuffer<ReflectionProbe> probes,
                                            uint probeCount,
                                            SamplerState sam,
                                            float3 posW,
                                            float3 R,
                                            float3 N,
                                            float prefilteredLod,
                                            uint fallbackIrradianceMapIdx,
                                            uint fallbackPrefilteredMapIdx,
                                            out float3 irradianceColor,
                                            out float3 prefilteredColor)
{
    uint best0 = 0.0f;
    uint best1 = 0.0f;
    float best0Dist = 1.0f;
    float best1Dist = 1.0f;
    
    // TODO: make tiled/clustered
    for (uint i = 0; i < probeCount; ++i)
    {
        ReflectionProbe p = probes[i];
        
        float3 local = posW - p.Position;

        // To avoid division, prefer to store inv extents on CPU: here we divide as original
        float3 d = abs(local) / p.BoxExtents;
        float volDist = max(d.x, max(d.y, d.z));
        
        float better0 = step(volDist, best0Dist);
        float better1 = step(volDist, best1Dist) * (1.0 - better0);

        best1Dist = lerp(best1Dist, best0Dist, better0);
        best1 = (uint) lerp(best1, best0, better0);

        best0Dist = lerp(best0Dist, volDist, better0);
        best0 = (uint) lerp(best0, i, better0);

        best1Dist = lerp(best1Dist, volDist, better1);
        best1 = (uint) lerp(best1, i, better1);
    }
    
    float w0 = saturate(1.0f - best0Dist);
    float w1 = saturate(1.0f - best1Dist);
    float sum = w0 + w1;
    
    if (sum < 1e-4)
    {
        ReflectionProbe sky;
        sky.IrradianceMapIdx = fallbackIrradianceMapIdx;
        sky.PrefilteredMapIdx = fallbackPrefilteredMapIdx;
        irradianceColor = SampleProbeIrradiance(sky, sam, N);
        prefilteredColor = SampleProbePrefilteredColor(sky, sam, R, prefilteredLod);
    }
    else
    {
        ReflectionProbe r0 = probes[best0];
        ReflectionProbe r1 = probes[best1];
        w0 /= sum;
        w1 /= sum;
        irradianceColor = w0 * SampleProbeIrradiance(r0, sam, N) + w1 * SampleProbeIrradiance(r1, sam, N);
        prefilteredColor = w0 * SampleProbePrefilteredColor(r0, sam, R, prefilteredLod) + w1 * SampleProbePrefilteredColor(r1, sam, R, prefilteredLod);
    }
}