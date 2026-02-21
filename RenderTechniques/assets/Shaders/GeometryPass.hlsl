SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    
    uint gAlbedoTexIdx;
    uint gNormalMapIdx;
    uint gORMIdx;
    uint gSSRMask;
}

cbuffer cbPass : register(b1)
{
    float4x4 gViewProj;
}

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float3 TangentU : TANGENT;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_POSITION0;
    float3 NormalW : NORMAL;
    float3 TangentW : TANGENT;
    float2 TexC : TEXCOORD;
};

struct PSOut
{
    half4 Albedo : SV_TARGET0;
#if PACK_NORMALS > 0
    half2 NormalW : SV_TARGET1;
#else
    half4 NormalW : SV_TARGET1;
#endif
    half4 ORM : SV_TARGET2;
};

#include "PackNormals.hlsli"
#include "CommonTransforms.hlsli"

VertexOut VS(VertexIn vIn)
{
    VertexOut output;
    
    output.PosH = mul(mul(float4(vIn.PosL, 1.0), gWorld), gViewProj);
    output.NormalW = TransformNormalToWorldSpace(vIn.NormalL, gWorld);
    output.TangentW = normalize(mul(vIn.TangentU, float3x3(gWorld[0].xyz, gWorld[1].xyz, gWorld[2].xyz)));
    output.TexC = vIn.TexC;
    
    return output;
}

PSOut PS(VertexOut vOut)
{
    Texture2D albedoTex = ResourceDescriptorHeap[gAlbedoTexIdx];
    
    PSOut psOut;
    psOut.Albedo = albedoTex.Sample(gsamLinearWrap, vOut.TexC);
    
    clip(psOut.Albedo.a - 0.1f);
    
    vOut.NormalW = normalize(vOut.NormalW);
    if (gNormalMapIdx != -1)
    {
        Texture2D normalMapTex = ResourceDescriptorHeap[gNormalMapIdx];
        float3 normalMapSample = normalMapTex.Sample(gsamPointWrap, vOut.TexC).xyz;
        float3 N = NormalSampleToWorldSpace(normalMapSample, vOut.NormalW, vOut.TangentW);
        N = normalize(N);
        
#if PACK_NORMALS > 0
        psOut.NormalW = normal_encode(N);
#else
        psOut.NormalW = float4(N, 0);
#endif
    }
    else
    {
#if PACK_NORMALS > 0
        psOut.NormalW = normal_encode(vOut.NormalW);
#else
        psOut.NormalW = float4(vOut.NormalW, 0);
#endif
    }
    
    if (gORMIdx != -1)
    {
        Texture2D ormTex = ResourceDescriptorHeap[gORMIdx];
        float3 orm = ormTex.Sample(gsamLinearWrap, vOut.TexC).rgb;
        psOut.ORM.rgb = orm.rgb;
    }
    else
    {
        psOut.ORM.rgb = float3(1.0f, 0.0f, 0.0f);
    }
    
    psOut.ORM.a = gSSRMask;
    
    return psOut;
}