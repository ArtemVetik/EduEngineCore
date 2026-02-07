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
    uint3 gPadding;
}

cbuffer cbPass : register(b1)
{
    float4x4 gViewProj;
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
    float2 TexC : TEXCOORD;
};

#include "CommonTransforms.hlsli"

VertexOut VS(VertexIn vIn)
{
    VertexOut output;
    
    output.PosH = mul(float4(vIn.PosL, 1.0f), gViewProj);
    output.TexC = vIn.TexC;
    
    return output;
}

float4 PS(VertexOut pIn) : SV_TARGET
{
    Texture2D albedo = ResourceDescriptorHeap[gAlbedoTexIdx];
    
    return albedo.Sample(gsamLinearWrap, pIn.TexC);
}