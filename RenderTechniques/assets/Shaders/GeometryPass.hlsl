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
    uint gMetallicRoughnessIdx;
    uint gAOIdx;
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
    half4 MetalRoughAo : SV_TARGET2;
};

//---------------------------------------------------------------------------------------
// Transforms a normal map sample to world space.
//---------------------------------------------------------------------------------------
float3 NormalSampleToWorldSpace(float3 normalMapSample, float3 unitNormalW, float3 tangentW)
{
	// Uncompress each component from [0,1] to [-1,1].
    float3 normalT = 2.0f * normalMapSample - 1.0f;

	// Build orthonormal basis.
    float3 N = unitNormalW;
    float3 T = normalize(tangentW - dot(tangentW, N) * N);
    float3 B = cross(N, T);

    float3x3 TBN = float3x3(T, B, N);

	// Transform from tangent space to world space.
    float3 bumpedNormalW = mul(normalT, TBN);

    return bumpedNormalW;
}

float3x3 InverseTranspose3x3(float3x3 M)
{
    // Note that in HLSL, M_t[0] is the first row, while in GLSL, it is the
    // first column. Luckily, determinant and inverse matrix can be equally
    // defined through both rows and columns.
    float det = dot(cross(M[0], M[1]), M[2]);
    float3x3 adjugate = float3x3(cross(M[1], M[2]),
                                 cross(M[2], M[0]),
                                 cross(M[0], M[1]));
    return adjugate / det;
}

#include "PackNormals.hlsl"

VertexOut VS(VertexIn vIn)
{
    VertexOut output;
    
    output.PosH = mul(mul(float4(vIn.PosL, 1.0), gWorld), gViewProj);
    
    float3x3 NormalTransform = float3x3(gWorld[0].xyz, gWorld[1].xyz, gWorld[2].xyz);
    NormalTransform = InverseTranspose3x3(NormalTransform);
    
    output.NormalW = normalize(mul(vIn.NormalL, NormalTransform));
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
        float3 normalMapSample = normalMapTex.Sample(gsamAnisotropicWrap, vOut.TexC).xyz;
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
    
    if (gMetallicRoughnessIdx != -1)
    {
        Texture2D metallicRoughnessTex = ResourceDescriptorHeap[gMetallicRoughnessIdx];
        float2 metallicRoughness = metallicRoughnessTex.Sample(gsamLinearWrap, vOut.TexC).gb;
        psOut.MetalRoughAo.r = metallicRoughness.g;
        psOut.MetalRoughAo.g = metallicRoughness.r;
    }
    else
    {
        psOut.MetalRoughAo.rg = float2(0.0f, 0.5f);
    }
    
    if (gAOIdx != -1)
    {
        Texture2D aoTex = ResourceDescriptorHeap[gAOIdx];
        psOut.MetalRoughAo.b = aoTex.Sample(gsamLinearWrap, vOut.TexC).r;
    }
    else
    {
        psOut.MetalRoughAo.b = 1;
    }
    
    return psOut;
}