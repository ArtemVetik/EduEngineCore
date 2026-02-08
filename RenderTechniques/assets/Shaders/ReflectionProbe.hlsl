SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);

cbuffer cbObject : register(b0)
{
    float4x4 gWorld;
    uint gAlbedoTexIdx;
    uint3 gPadding0;
}

cbuffer cbPass : register(b1)
{
    float4x4 gViewProj;
    float4x4 gShadowTransform;
    uint gShadowmapIdx;
    uint3 gPadding1;
}

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NornalL : NORMAL;
    float3 TangentL : TANGENT;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 ShadowPosH : POSITION;
    float2 TexC : TEXCOORD;
};

#include "CommonTransforms.hlsli"

VertexOut VS(VertexIn vIn)
{
    VertexOut output;
    
    float4 posW = mul(float4(vIn.PosL, 1.0f), gWorld);
    
    output.PosH = mul(posW, gViewProj);
    output.ShadowPosH = mul(posW, gShadowTransform);
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
    shadowAttenuation.x = shadowMap.SampleCmpLevelZero(gsamShadow, pIn.ShadowPosH.xy + float2(-dx, -dx), pIn.ShadowPosH.z);;
    shadowAttenuation.y = shadowMap.SampleCmpLevelZero(gsamShadow, pIn.ShadowPosH.xy + float2( dx,  dx), pIn.ShadowPosH.z);;
    shadowAttenuation.z = shadowMap.SampleCmpLevelZero(gsamShadow, pIn.ShadowPosH.xy + float2( dx, -dx), pIn.ShadowPosH.z);;
    shadowAttenuation.w = shadowMap.SampleCmpLevelZero(gsamShadow, pIn.ShadowPosH.xy + float2( dx,  dx), pIn.ShadowPosH.z);;
    float shadowFactor = dot(shadowAttenuation, float(0.25));
    
    float4 albedoColor = albedoTex.Sample(gsamLinearWrap, pIn.TexC);
    
    return albedoColor * 0.2f + shadowFactor * albedoColor;
}