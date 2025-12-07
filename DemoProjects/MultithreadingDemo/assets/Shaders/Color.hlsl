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
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_Position;
    float2 TexC : TEXCOORD;
};

VertexOut VS(VertexIn vIn)
{
    VertexOut output;
    
    output.PosH = mul(mul(float4(vIn.PosW, 1.0), gWorld), gViewProj);
    output.TexC = vIn.TexC;
    
    return output;
}

float4 PS(VertexOut vOut) : SV_Target
{
    return gAlbedo.Sample(gsamLinearWrap, vOut.TexC);
}