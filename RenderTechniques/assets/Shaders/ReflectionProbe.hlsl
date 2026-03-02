#include "Lighting.hlsli"
#include "CommonTransforms.hlsli"

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);

StructuredBuffer<Light> gLights : register(t0);

cbuffer cbObject : register(b0)
{
    float4x4 gWorld;
    uint gAlbedoTexIdx;
    uint gNormalMapIdx;
    uint2 gPadding0;
}

cbuffer cbPass : register(b1)
{
    float4x4 gViewProj;
    float4x4 gShadowTransform;
    uint gShadowmapIdx;
    uint gDirectionalLightCount;
    uint2 gPadding;
}

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NornalL : NORMAL;
    float3 TangentU : TANGENT;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 NormalW : NORMAL;
    float3 TangentW : TANGENT;
    float3 ShadowPosH : POSITION;
    float2 TexC : TEXCOORD;
};

VertexOut VS(VertexIn vIn)
{
    VertexOut output;
    
    float4 posW = mul(float4(vIn.PosL, 1.0f), gWorld);
    
    output.PosH = mul(posW, gViewProj);
    output.NormalW = TransformNormalToWorldSpace(vIn.PosL, gWorld);
    output.TangentW = normalize(mul(vIn.TangentU, float3x3(gWorld[0].xyz, gWorld[1].xyz, gWorld[2].xyz)));
    output.ShadowPosH = mul(posW, gShadowTransform).xyz;
    output.TexC = vIn.TexC;
    
    return output;
}

float4 PS(VertexOut pIn) : SV_TARGET
{
    Texture2D albedoTex = ResourceDescriptorHeap[gAlbedoTexIdx];
    Texture2D shadowMap = ResourceDescriptorHeap[gShadowmapIdx];
    
    uint width, height, numMips;
    shadowMap.GetDimensions(0, width, height, numMips);
	
    float dx = 1.0f / (float) width;
    dx = 0.5 * dx;
    
    float4 shadowAttenuation;
    shadowAttenuation.x = shadowMap.SampleCmpLevelZero(gsamShadow, pIn.ShadowPosH.xy + float2(-dx, -dx), pIn.ShadowPosH.z);
    shadowAttenuation.y = shadowMap.SampleCmpLevelZero(gsamShadow, pIn.ShadowPosH.xy + float2(-dx,  dx), pIn.ShadowPosH.z);
    shadowAttenuation.z = shadowMap.SampleCmpLevelZero(gsamShadow, pIn.ShadowPosH.xy + float2( dx, -dx), pIn.ShadowPosH.z);
    shadowAttenuation.w = shadowMap.SampleCmpLevelZero(gsamShadow, pIn.ShadowPosH.xy + float2( dx,  dx), pIn.ShadowPosH.z);
    float shadowFactor = dot(shadowAttenuation, float(0.25));
    
    float3 albedoColor = albedoTex.Sample(gsamLinearWrap, pIn.TexC).xyz;
    
    float3 N = 0;
    
    if (gNormalMapIdx != -1)
    {
        Texture2D normalMapTex = ResourceDescriptorHeap[gNormalMapIdx];
        float3 normalMapSample = normalMapTex.Sample(gsamPointWrap, pIn.TexC).xyz;
        N = NormalSampleToWorldSpace(normalMapSample, pIn.NormalW, pIn.TangentW);
        N = normalize(N);
    }
    else
    {
        N = normalize(pIn.NormalW);
    }
    
    float3 color = 0.0f;
    
    for (int i = 0; i < gDirectionalLightCount; i++)
    {
        float3 L = normalize(-gLights[i].Direction);
        float3 NdotL = max(dot(N, L), 0);
        
        color += NdotL * albedoColor.xyz * gLights[i].Color;
    }
    
    return float4(albedoColor * 0.2f + shadowFactor * color, 1.0f);
}