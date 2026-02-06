cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
};

cbuffer cbPass : register(b1)
{
    float4x4 gViewProj;
    float3 gLightDirection;
    float gPadding;
    float4 gShadowBias;
};

struct VertexIn
{
	float3 PosL     : POSITION;
	float3 NormalL  : NORMAL;
	float3 TangentL : TANGENT;
	float2 TexC     : TEXCOORD;
};

struct VertexOut
{
	float4 PosH    : SV_POSITION;
	float2 TexC    : TEXCOORD;
};

#include "CommonTransforms.hlsli"

float3 ApplyShadowBias(float3 positionWS, float3 normalWS, float3 lightDirection)
{
    float invNdotL = 1.0 - saturate(dot(lightDirection, normalWS));
    float scale = invNdotL * gShadowBias.y;

    // normal bias is negative since we want to apply an inset normal offset
    positionWS = lightDirection * gShadowBias.xxx + positionWS;
    positionWS = normalWS * scale.xxx + positionWS;
    return positionWS;
}

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut) 0.0f;
	
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    float3 normalW = TransformNormalToWorldSpace(vin.NormalL, gWorld);
    
    posW.xyz = ApplyShadowBias(posW.xyz, normalW, gLightDirection);
    
    vout.PosH = mul(posW, gViewProj);
    vout.TexC = vin.TexC;
	
    return vout;
}


