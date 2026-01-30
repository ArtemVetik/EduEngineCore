Texture2D gAlbedo : register(t0);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
}

cbuffer cbPass : register(b1)
{
    float4x4 gViewProj;
}

struct VertexIn
{
    float3 PosW : POSITION;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_Position;
    float2 TexC : TEXCOORD;
    float3 NormalW : NORMAL;
};

struct PSOut
{
    float4 Color : SV_TARGET0;
    float4 NormalW : SV_TARGET1;
};

VertexOut VS(VertexIn vIn)
{
    VertexOut output;
    
    output.PosH = mul(mul(float4(vIn.PosW, 1.0), gWorld), gViewProj);
    output.TexC = vIn.TexC;
    output.NormalW = mul(float4(vIn.Normal, 0.0), gWorld);
    
    return output;
}

PSOut PS(VertexOut vOut)
{
    PSOut psOut;
    
    float3 L = normalize(float3(0, 1, -1));
    float3 N = normalize(vOut.NormalW);
    
    float4 color = gAlbedo.Sample(gsamLinearWrap, vOut.TexC);
    
    //color = color * (dot(N, L) + 0.5f);
    clip(color.a - 0.01f);
    
    psOut.Color = color;
    psOut.NormalW = float4(normalize(vOut.NormalW), 0);
    
    return psOut;
}