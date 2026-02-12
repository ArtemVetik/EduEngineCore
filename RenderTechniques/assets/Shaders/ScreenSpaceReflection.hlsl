SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);

static const int gBlurRadius = 5;

cbuffer cbPass : register(b0)
{
    float4x4 gView;
    float4x4 gProj;
    float4x4 gInvProj;
}

cbuffer cbConstants : register(b1)
{
    uint2 gScreenSize;
    uint gInputColorTexIdx;
    uint gNormalTexIdx;
    
    uint gMaskTexIdx;
    uint gDepthTexIdx;
    uint gSSRTexIdx0;
    uint gSSRTexIdx1;
    
    uint gMaxIterations;
    float gDepthThickness;
    uint2 gPadding;
    
    float4 gBlurWeights[3];
}

cbuffer cbBlurPass : register(b2)
{
    uint gHorizontalBlur;
}

#include "CommonTransforms.hlsli"
#include "PackNormals.hlsli"

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
void CS_Main(uint2 tid : SV_DispatchThreadID)
{
    if (tid.x >= gScreenSize.x || tid.y >= gScreenSize.y)
        return;
    
    float4 finalColor = 0;

    Texture2D<float4> albedoTex = ResourceDescriptorHeap[gInputColorTexIdx];
    Texture2D<float4> maskTex = ResourceDescriptorHeap[gMaskTexIdx];
    Texture2D<float4> depthTex = ResourceDescriptorHeap[gDepthTexIdx];
    RWTexture2D<float4> outTex = ResourceDescriptorHeap[gSSRTexIdx0];
    
    float2 texC = (float2(tid) + 0.5) / gScreenSize;
    
#if PACK_NORMALS > 0
    Texture2D<float2> normalTex = ResourceDescriptorHeap[gNormalTexIdx];
    float2 normalPacked = normalTex.Sample(gsamPointWrap, texC).xy;
    float3 normalInWS = normal_decode(normalPacked);
#else
    Texture2D<float4> normalTex = ResourceDescriptorHeap[gNormalTexIdx];
    float3 normalInWS = normalTex.Sample(gsamPointWrap, texC).xyz;
#endif
    normalInWS = normalize(normalInWS);
    
    float reflectionMask = maskTex.Sample(gsamPointWrap, texC).w;
    
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
    
    outTex[tid] = reflectionColor;
}

//
// Blur Pass
//

float NdcDepthToViewDepth(float z_ndc)
{
    // z_ndc = A + B/viewZ, where gProj[2,2]=A and gProj[3,2]=B.
    float viewZ = gProj[3][2] / (z_ndc - gProj[2][2]);
    return viewZ;
}

float3 SampleNormal(uint3 tid)
{
#if PACK_NORMALS > 0
    Texture2D<float2> normalTex = ResourceDescriptorHeap[gNormalTexIdx];
    float2 normalPacked = normalTex.Load(tid).xy;
    float3 n = normal_decode(normalPacked);
#else
    Texture2D<float4> normalTex = ResourceDescriptorHeap[gNormalTexIdx];
    float3 n = normalTex.Load(tid).xyz;
#endif
    
    n = mul(n, (float3x3)gView);
    return n;
}

[numthreads(8, 8, 1)]
void CS_Blur(uint3 tid : SV_DispatchThreadID)
{
    if (tid.x >= gScreenSize.x || tid.y >= gScreenSize.y)
        return;
    
    float blurWeights[12] =
    {
        gBlurWeights[0].x, gBlurWeights[0].y, gBlurWeights[0].z, gBlurWeights[0].w,
        gBlurWeights[1].x, gBlurWeights[1].y, gBlurWeights[1].z, gBlurWeights[1].w,
        gBlurWeights[2].x, gBlurWeights[2].y, gBlurWeights[2].z, gBlurWeights[2].w,
    };
    
    uint3 pixelOffset = gHorizontalBlur ? uint3(1, 0, 0) : uint3(0, 1, 0);
    uint inputTexId = gHorizontalBlur ? gSSRTexIdx0 : gSSRTexIdx1;
    uint outTexId = gHorizontalBlur ? gSSRTexIdx1 : gSSRTexIdx0;
    
    Texture2D<float4> inputTex = ResourceDescriptorHeap[inputTexId];
    Texture2D<float4> depthTex = ResourceDescriptorHeap[gDepthTexIdx];
    RWTexture2D<float4> outTex = ResourceDescriptorHeap[outTexId];
    
    float4 color = blurWeights[gBlurRadius] * inputTex.Load(tid);
    float totalWeight = blurWeights[gBlurRadius];
    
    float3 centerNormal = SampleNormal(tid);
    
    float centerDepth = NdcDepthToViewDepth(depthTex.Load(tid).r);

    for (int i = -gBlurRadius; i <= gBlurRadius; ++i)
    {
		// We already added in the center weight.
        if (i == 0)
            continue;
        
        uint3 tid2 = tid + i * pixelOffset;

        if (tid2.x < 0 || tid2.y < 0 ||
            tid2.x >= gScreenSize.x ||
            tid2.y >= gScreenSize.y)
            continue;
        
        half3 neighborNormal = SampleNormal(tid2);

        float neighborDepth = NdcDepthToViewDepth(depthTex.Load(tid2).r);

		//
		// If the center value and neighbor values differ too much (either in 
		// normal or depth), then we assume we are sampling across a discontinuity.
		// We discard such samples from the blur.
		//
	
        if (dot(neighborNormal, centerNormal) >= 0.8f &&
		    abs(neighborDepth - centerDepth) <= 0.2f)
        {
            float weight = blurWeights[i + gBlurRadius];
            
            color += weight * inputTex.Load(tid2);
		
            totalWeight += weight;
        }
    }

	// Compensate for discarded samples by making total weights sum to 1.
    outTex[tid.xy] = color / totalWeight;
}
