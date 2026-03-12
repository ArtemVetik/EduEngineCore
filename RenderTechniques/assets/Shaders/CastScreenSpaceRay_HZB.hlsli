#define SSRT_SAMPLE_BATCH_SIZE 4
#define FarDepthValue 0.0f

struct FSSRTRay
{
    float3 RayStartScreen;
    float3 RayStepScreen;
    float CompareTolerance;
};

struct FSSRTCastingSettings
{
    bool bStopWhenUncertain;
};

struct FViewParams
{
    float4 ScreenPositionScaleBias;
    float4 HZBUvFactorAndInvFactor;
};

void CastScreenSpaceRay_HZB(
    Texture2D<float4> DepthHZB, SamplerState Sampler,
    float StartMipLevel,
    FSSRTCastingSettings CastSettings,
    FSSRTRay Ray,
    float Roughness,
    uint NumSteps, float StepOffset,
    float4 InHZBUvFactorAndInvFactor,
    FViewParams View,
    out float3 DebugOutput,
    out float3 OutHitUVz,
    out float Level,
    out bool bFoundHit,
    out bool bUncertain)
{
    DebugOutput = float3(0, 0, 0);
    
    // convert from screen texture space to HZB texture space
    float3 RayStartUVz = float3(Ray.RayStartScreen.xy * InHZBUvFactorAndInvFactor.xy, Ray.RayStartScreen.z);
    float3 RayStepUVz = float3(Ray.RayStepScreen.xy * InHZBUvFactorAndInvFactor.xy, Ray.RayStepScreen.z);
    
    const float Step = 1.0 / (float) NumSteps;
    float CompareTolerance = Ray.CompareTolerance * Step;

    float LastDiff = 0;
    Level = StartMipLevel;
    
    RayStepUVz *= Step;
    float3 RayUVz = RayStartUVz + RayStepUVz * StepOffset;

    uint MaxIteration = NumSteps;
    
    bFoundHit = false;
    bUncertain = false;
    
    float4 MultipleSampleDepthDiff;
    bool4 bMultipleSampleHit;
    
    uint i = 0;
    for (i = 0; i < MaxIteration; i += SSRT_SAMPLE_BATCH_SIZE)
    {
        float2 SamplesUV[SSRT_SAMPLE_BATCH_SIZE];
        float4 SamplesZ = 0;
        float4 SamplesMip = 0;
        
        [unroll]
        for (uint j = 0; j < SSRT_SAMPLE_BATCH_SIZE; ++j)
        {
            float idx = float(i) + float(j + 1);
            SamplesUV[j] = RayUVz.xy + idx * RayStepUVz.xy;
            SamplesZ[j] = RayUVz.z + idx * RayStepUVz.z;
        }
        
        SamplesMip.xy = Level;
        Level += (8.0 / (float) NumSteps) * Roughness;
        SamplesMip.zw = Level;
        Level += (8.0 / (float) NumSteps) * Roughness;
        
        float4 SampleDepth = float4(0, 0, 0, 0);
        [unroll]
        for (uint j = 0; j < SSRT_SAMPLE_BATCH_SIZE; ++j)
        {
            SampleDepth[j] = DepthHZB.SampleLevel(Sampler, SamplesUV[j], SamplesMip[j]).r;
        }

        // Evaluate intersections: depth difference = raySampleDepth - sceneDepth
        MultipleSampleDepthDiff = SamplesZ - SampleDepth;

        // Hit if abs(diff + tol) < tol  AND sampled depth != far
        bMultipleSampleHit = abs(MultipleSampleDepthDiff + CompareTolerance) < CompareTolerance;
        [unroll]
        for (uint j = 0; j < SSRT_SAMPLE_BATCH_SIZE; ++j)
        {
            bMultipleSampleHit[j] = bMultipleSampleHit[j] && (SampleDepth[j] != FarDepthValue);
        }

        // Uncertain if depthDiff + tol < -tol (we are behind geometry -> ray might go behind)
        bool4 bMultipleSampleUncertain = (MultipleSampleDepthDiff + CompareTolerance) < -CompareTolerance;

        // reduce per-batch to scalar flags
        [unroll]
        for (uint j = 0; j < SSRT_SAMPLE_BATCH_SIZE; ++j)
        {
            bFoundHit = bFoundHit || bMultipleSampleHit[j];
            // note: in original UE there was logic to set uncertain only if not found yet
            bool bLocalMultisampleUncertain = bMultipleSampleUncertain[j];
            bUncertain = bUncertain || (bLocalMultisampleUncertain && !bFoundHit);
        }
        
        if (bFoundHit || (CastSettings.bStopWhenUncertain && bUncertain))
        {
            break;
        }

        // update LastDiff for next batch interpolation
        LastDiff = MultipleSampleDepthDiff.w;
    } // for
    
    if (bFoundHit)
    {
        // Find closest hit index in this batch: smallest idx (j) where bMultipleSampleHit[j] true.
        // Also capture LastDiff = depth diff of previous trailing sample for interpolation.
        float Time0_local = 3.0;
        float DepthDiff0 = MultipleSampleDepthDiff[2];
        float DepthDiff1 = MultipleSampleDepthDiff[3];
            
        if (bMultipleSampleHit[2])
        {
            DepthDiff0 = MultipleSampleDepthDiff[1];
            DepthDiff1 = MultipleSampleDepthDiff[2];
            Time0_local = 2.0;
        }
        if (bMultipleSampleHit[1])
        {
            DepthDiff0 = MultipleSampleDepthDiff[0];
            DepthDiff1 = MultipleSampleDepthDiff[1];
            Time0_local = 1.0;
        }
        if (bMultipleSampleHit[0])
        {
            DepthDiff0 = LastDiff;
            DepthDiff1 = MultipleSampleDepthDiff[0];
            Time0_local = 0.0;
        }

        Time0_local += float(i);

        float Time1_local = Time0_local + 1.0;
            
        float denom = (DepthDiff0 - DepthDiff1);
        float TimeLerp = (abs(denom) > 1e-6) ? saturate(DepthDiff0 / denom) : 0.0;

        float IntersectTime = Time0_local + TimeLerp;

        OutHitUVz = RayUVz + RayStepUVz * IntersectTime;

        // Debug print point (optional)
        DebugOutput.x = IntersectTime / float(NumSteps);
    }
    else
    {
        OutHitUVz = RayUVz + RayStepUVz * float(i);
    }
    
    // convert OutHitUVz.xy back to screen texture space
    OutHitUVz.xy *= InHZBUvFactorAndInvFactor.zw;
}
