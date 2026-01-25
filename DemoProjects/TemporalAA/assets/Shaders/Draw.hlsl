Texture2D gAlbedo : register(t0);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

cbuffer cbPerObject : register(b0)
{
    float4x4 gCurrWorld;
    float4x4 gPrevWorld;
}

cbuffer cbPass : register(b1)
{
    float4x4 gCurrViewProj;
    float4x4 gCurrViewProjNoJitter;
    float4x4 gPrevViewProjNoJitter;
    float2 gRTSize;
    uint2 gPadding;
}

struct VertexIn
{
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float3 TangentW : TANGENT;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD0;
    float4 CurrClip : TEXCOORD1;
    float4 PrevClip : TEXCOORD2;
};

struct PSOutput
{
    float4 Color : SV_Target0;
    float2 Velocity : SV_Target1;
};

VertexOut VS(VertexIn vIn)
{
    VertexOut output;
    
    float4 currWorld = mul(float4(vIn.PosW, 1), gCurrWorld);
    float4 prevWorld = mul(float4(vIn.PosW, 1), gPrevWorld);
    
    output.PosH = mul(currWorld, gCurrViewProj);
    output.TexC = vIn.TexC;
    
    float4 currClip = mul(currWorld, gCurrViewProjNoJitter);
    float4 prevClip = mul(prevWorld, gPrevViewProjNoJitter);

    output.CurrClip = currClip;
    output.PrevClip = prevClip;
    
    return output;
}

PSOutput PS(VertexOut vOut)
{
    PSOutput output;
    
    float4 albedo = gAlbedo.Sample(gsamLinearWrap, vOut.TexC);
    
    output.Color = pow(albedo, 2.2f);
    
    float2 currNDC = vOut.CurrClip.xy / vOut.CurrClip.w;
    float2 prevNDC = vOut.PrevClip.xy / vOut.PrevClip.w;

    float2 currUV = currNDC * float2(0.5, -0.5) + 0.5;
    float2 prevUV = prevNDC * float2(0.5, -0.5) + 0.5;

    float2 velocity = currUV - prevUV;
    
    output.Velocity = velocity;
    
    return output;
}