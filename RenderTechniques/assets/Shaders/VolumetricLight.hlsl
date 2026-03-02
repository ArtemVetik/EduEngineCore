#define PI 3.1415926536

#include "CommonTransforms.hlsli"
#include "Shadows.hlsli"

SamplerState gsamPointClamp : register(s1);
SamplerComparisonState gsamShadow : register(s6);

cbuffer cbPass : register(b0)
{
    float4x4 gInvView;
    float4x4 gInvProj;
    float3 gLightDir;
    uint gDepthTexIdx;
    
    float4x4 gCascadeTransform[MAX_CASCADES];
    float4 gCascadeShadowSphere[MAX_CASCADES];
    float4 gCascadeShadowRad2;
    uint4 gShadowMapIdx;
    uint gCascadeCount;
    float3 gCameraPos;
    
    uint gNumSteps;
    float gDensity;
    float gAbsorption;
    float gIntensity;
}

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

float CalcVisibility(float3 posW)
{
    uint cascadeIndex = ComputeCascadeIndex(posW, gCascadeShadowSphere, gCascadeShadowRad2);
    
    if (cascadeIndex < gCascadeCount)
    {
        float4 shadowPosH = mul(float4(posW, 1), gCascadeTransform[cascadeIndex]);
        
        Texture2D shadowMap = ResourceDescriptorHeap[gShadowMapIdx[cascadeIndex]];
        return shadowMap.SampleCmpLevelZero(gsamShadow, shadowPosH.xy, shadowPosH.z).r;
    }
    
    return 0;
}

// See: https://www.pbr-book.org/3ed-2018/Volume_Scattering/Phase_Functions#PhaseHG
float PhaseHG(float cosTheta, float g)
{
    float g2 = g * g;
    float denom = 1.0 + g2 - 2.0 * g * cosTheta;
    return (1.0 - g2) / (4.0 * PI * denom * sqrt(denom));
}

float PS(VertexOut pIn) : SV_TARGET
{
    Texture2D depthTex = ResourceDescriptorHeap[gDepthTexIdx];
    float depth = depthTex.Sample(gsamPointClamp, pIn.TexC).r;
    float3 posW = ReconstructWorldPosFromDepth(depth, pIn.TexC, gInvProj, gInvView).xyz;
    
    float3 camPos = gCameraPos;
    float3 viewDir = normalize(posW - camPos);
    float3 lightDir = normalize(gLightDir);
    
    float rayLength = length(posW - camPos);
    float stepLength = rayLength / gNumSteps;

    float3 samplePos = camPos;

    float scattering = 0;
    float transmittance = 1.0;

    for (int i = 0; i < gNumSteps; i++)
    {
        samplePos += viewDir * stepLength;
        
        float visibility = CalcVisibility(samplePos);
        
        float cosTheta = dot(-lightDir, viewDir);
        float phase = PhaseHG(cosTheta, 0.2);

        float localScattering = visibility * gDensity * phase;

        scattering += transmittance * localScattering * stepLength;

        transmittance *= exp(-gAbsorption * stepLength);

        if (transmittance < 0.01)
            break;
    }
    
    return scattering * gIntensity;
}