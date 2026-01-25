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
    float3 Normal : NORMAL;
};

struct VertexOut
{
    float4 PosH : SV_Position;
    float2 TexC : TEXCOORD;
    float3 Normal : NORMAL;
};

VertexOut VS(VertexIn vIn)
{
    VertexOut output;
    
    output.PosH = mul(mul(float4(vIn.PosW, 1.0), gWorld), gViewProj);
    output.TexC = vIn.TexC;
    output.Normal = mul(float4(vIn.Normal, 0.0), gWorld);
    
    return output;
}

float4 PS(VertexOut vOut) : SV_Target
{
    float3 L = normalize(float3(0, 1, -1));
    float3 N = normalize(vOut.Normal);
    
    float4 color = gAlbedo.Sample(gsamLinearWrap, vOut.TexC);
    
    color = color * (dot(N, L) + 0.3f);
    
    return color;
}