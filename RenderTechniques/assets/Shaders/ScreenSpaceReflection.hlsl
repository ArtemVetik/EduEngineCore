SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);

cbuffer cbPass : register(b0)
{
    float4x4 gView;
    float4x4 gProj;
    float4x4 gInvProj;
    uint2 gScreenSize;
    uint gAlbedoTexIdx;
    uint gNormalTexIdx;
    uint gMaskTexIdx;
    uint gDepthTexIdx;
    uint gOutTexIdx;
    uint gMaxIterations;
    float gDepthThickness;
}

#include "CommonTransforms.hlsli"

void ComputePosAndReflection(uint2 tid, float3 normalVS, Texture2D<float4> depthTex, out float3 outSamplePosInTS, out float3 outReflDirInTS, out float outMaxDistance)
{
    float sampleDepth = depthTex.Load(int3(tid, 0)).r;
    float texC = (float2(tid) + 0.5) / gScreenSize;
    
    float4 samplePosInCS = float4(((float2(tid) + 0.5) / gScreenSize) * 2 - 1.0f, sampleDepth, 1);
    samplePosInCS.y *= -1;

    float4 samplePosInVS = mul(samplePosInCS, gInvProj);
    samplePosInVS /= samplePosInVS.w;
    
    float3 vCamToSampleInVS = normalize(samplePosInVS.xyz);
    float4 vReflectionInVS = float4(reflect(vCamToSampleInVS, normalVS), 0);
    
    float4 vReflectionEndPosInVS = samplePosInVS + vReflectionInVS * 1000;
    vReflectionEndPosInVS /= (vReflectionEndPosInVS.z < 0 ? vReflectionEndPosInVS.z : 1);
    float4 vReflectionEndPosInCS = mul(float4(vReflectionEndPosInVS.xyz, 1), gProj);
    vReflectionEndPosInCS /= vReflectionEndPosInCS.w;
    float3 vReflectionDir = normalize((vReflectionEndPosInCS - samplePosInCS).xyz);

    // Transform to texture space
    samplePosInCS.xy *= float2(0.5f, -0.5f);
    samplePosInCS.xy += float2(0.5f, 0.5f);
    
    vReflectionDir.xy *= float2(0.5f, -0.5f);
    
    outSamplePosInTS = samplePosInCS.xyz;
    outReflDirInTS = vReflectionDir;
    
	// Compute the maximum distance to trace before the ray goes outside of the visible area.
    // find t, so that:
    //      samplePos.x + dir.x * t = boundary
    //      t = (boundary - samplePos.x) / dir.x
    // boundary = 1 if dir.x > 0, 0 if dir.x < 0
    outMaxDistance = outReflDirInTS.x >= 0 ? (1 - outSamplePosInTS.x) / outReflDirInTS.x : -outSamplePosInTS.x / outReflDirInTS.x;
    outMaxDistance = min(outMaxDistance, outReflDirInTS.y < 0 ? (-outSamplePosInTS.y / outReflDirInTS.y) : ((1 - outSamplePosInTS.y) / outReflDirInTS.y));
    outMaxDistance = min(outMaxDistance, outReflDirInTS.z < 0 ? (-outSamplePosInTS.z / outReflDirInTS.z) : ((1 - outSamplePosInTS.z) / outReflDirInTS.z));
}

float FindIntersection_Linear(Texture2D<float4> depthTex, float3 samplePosInTS, float3 vReflDirInTS, float maxTraceDistance, out float3 intersection)
{
    float3 vReflectionEndPosInTS = samplePosInTS + vReflDirInTS * maxTraceDistance;
    
    float3 dp = vReflectionEndPosInTS.xyz - samplePosInTS.xyz;
    int2 sampleScreenPos = int2(samplePosInTS.xy * gScreenSize);
    int2 endPosScreenPos = int2(vReflectionEndPosInTS.xy * gScreenSize);
    int2 dp2 = endPosScreenPos - sampleScreenPos;
    //const int max_dist = max(abs(dp2.x), abs(dp2.y));
    const int max_dist = max(1, max(abs(dp2.x), abs(dp2.y)));
    dp /= max_dist;
    
    float4 rayPosInTS = float4(samplePosInTS.xyz + dp, 0);
    float4 vRayDirInTS = float4(dp.xyz, 0);
    float4 rayStartPos = rayPosInTS;
    
    int hitIndex = -1;
    for (int i = 0; i <= max_dist && i < gMaxIterations; i += 4)
    {
        float depth0 = 0;
        float depth1 = 0;
        float depth2 = 0;
        float depth3 = 0;

        float4 rayPosInTS0 = rayPosInTS + vRayDirInTS * 0;
        float4 rayPosInTS1 = rayPosInTS + vRayDirInTS * 1;
        float4 rayPosInTS2 = rayPosInTS + vRayDirInTS * 2;
        float4 rayPosInTS3 = rayPosInTS + vRayDirInTS * 3;
        
        depth3 = depthTex.Sample(gsamPointClamp, rayPosInTS3.xy).x;
        depth2 = depthTex.Sample(gsamPointClamp, rayPosInTS2.xy).x;
        depth1 = depthTex.Sample(gsamPointClamp, rayPosInTS1.xy).x;
        depth0 = depthTex.Sample(gsamPointClamp, rayPosInTS0.xy).x;

        {
            float thickness = depth3 - rayPosInTS3.z;
            hitIndex = (thickness >= 0 && thickness < gDepthThickness) ? (i + 3) : hitIndex;
        }
        {
            float thickness = depth2 - rayPosInTS2.z;
            hitIndex = (thickness >= 0 && thickness < gDepthThickness) ? (i + 2) : hitIndex;
        }
        {
            float thickness = depth1 - rayPosInTS1.z;
            hitIndex = (thickness >= 0 && thickness < gDepthThickness) ? (i + 1) : hitIndex;
        }
        {
            float thickness = depth0 - rayPosInTS0.z;
            hitIndex = (thickness >= 0 && thickness < gDepthThickness) ? (i + 0) : hitIndex;
        }

        if (hitIndex != -1)
            break;

        rayPosInTS = rayPosInTS3 + vRayDirInTS;
    }

    bool intersected = hitIndex >= 0;
    intersection = rayStartPos.xyz + vRayDirInTS.xyz * hitIndex;
	
    float intensity = intersected ? 1 : 0;
	
    return intensity;
}

float4 ComputeReflectedColor(Texture2D<float4> albedoTex, float intensity, float3 intersection, float4 skyColor)
{
    float4 ssr_color = albedoTex.Sample(gsamPointWrap, intersection.xy);
	
    return lerp(skyColor, ssr_color, intensity);
}

[numthreads(32, 32, 1)]
void CS(uint2 tid : SV_DispatchThreadID)
{
    if (tid.x >= gScreenSize.x || tid.y >= gScreenSize.y)
        return;
    
    float4 finalColor = 0;

    Texture2D<float4> albedoTex = ResourceDescriptorHeap[gAlbedoTexIdx];
    Texture2D<float4> normalTex = ResourceDescriptorHeap[gNormalTexIdx];
    Texture2D<float4> maskTex = ResourceDescriptorHeap[gMaskTexIdx];
    Texture2D<float4> depthTex = ResourceDescriptorHeap[gDepthTexIdx];
    RWTexture2D<float4> outTex = ResourceDescriptorHeap[gOutTexIdx];
    
    float2 texC = (float2(tid) + 0.5) / gScreenSize;
    
    float3 normalInWS = normalTex.Sample(gsamPointWrap, texC).xyz;
    float reflectionMask = maskTex.Sample(gsamPointWrap, texC).w;
    float4 color = albedoTex.Sample(gsamPointWrap, texC);
    
    normalInWS = normalize(normalInWS);
    
    float3 normalVS = mul(float4(normalInWS, 0.0f), gView).xyz;
    normalVS = normalize(normalVS);
    
    float4 skyColor = float4(0, 0, 0, 0);
	
    float4 reflectionColor = 0;
    
    if (reflectionMask != 0)
    {
        reflectionColor = skyColor;
        float3 samplePosInTS = 0;
        float3 vReflDirInTS = 0;
        float maxTraceDistance = 0;

        ComputePosAndReflection(tid, normalVS, depthTex, samplePosInTS, vReflDirInTS, maxTraceDistance);

        float3 intersection = 0;
        float intensity = FindIntersection_Linear(depthTex, samplePosInTS, vReflDirInTS, maxTraceDistance, intersection);
		
        reflectionColor = ComputeReflectedColor(albedoTex, intensity, intersection, skyColor);
    }
    
    finalColor = color + reflectionColor;
    
    outTex[tid] = finalColor;
}