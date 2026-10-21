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
    float3 gLightDirection;
    float gAmbientStrength;
    float3 gLightColor;
    uint gDebugMode;
}

cbuffer cbBuffers : register(b2)
{
    uint gAlbedoTex;
    uint gNormalTex;
    uint gAccumTex;
    uint gShadowMask;
    uint gDebugMask;
}

struct VertexIn
{
    float3 PosW : POSITION;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
};

struct VertexOut
{
    float4 PosH : SV_Position;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float4 Color : COLOR;
};

struct VertexOutFS // TODO: from FSquadVS.hlsl
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

struct PSOut
{
    half4 Albedo : SV_TARGET0;
    half4 NormalW : SV_TARGET1;
};

VertexOut VS(VertexIn vIn)
{
    VertexOut output;
    
    float4 posW = mul(float4(vIn.PosW, 1.0), gWorld);
    
    output.PosW = posW.xyz;
    output.PosH = mul(posW, gViewProj);
    output.NormalW = mul(float4(vIn.Normal, 0.0), gWorld).xyz;
    output.Color = vIn.Color;
    
    return output;
}

PSOut PS_Geometry(VertexOut vOut)
{
    PSOut output;
    output.Albedo = vOut.Color;
    output.NormalW = float4(vOut.NormalW, 0);
    
    return output;
}

float4 PS_Lighting(VertexOutFS vOut) : SV_TARGET
{
    Texture2D<float> shadowMask = ResourceDescriptorHeap[gShadowMask];
    Texture2D<half4> albedoTex = ResourceDescriptorHeap[gAlbedoTex];
    Texture2D<half4> normalTex = ResourceDescriptorHeap[gNormalTex];
    
    half4 albedo = albedoTex.Sample(gsamPointClamp, vOut.TexC);
    
    float3 L = normalize(-gLightDirection);
    float3 N = normalize(normalTex.Sample(gsamPointClamp, vOut.TexC).xyz);
    
    float4 ambient = gAmbientStrength * albedo;
    float shadowFactor = shadowMask.Sample(gsamPointClamp, vOut.TexC);

    float4 color = ambient + albedo * float4(gLightColor, 1) * saturate(dot(N, L)) * shadowFactor;
    
    return color;
}

float4 PS_PostProc(VertexOutFS vOut) : SV_TARGET
{
    Texture2D<float4> accumTex = ResourceDescriptorHeap[gAccumTex];
    
    float3 color = accumTex.Sample(gsamPointClamp, vOut.TexC).xyz;
    color = color / (color + 1);
    color = pow(color, 1.0f / 2.2f);
    
    if (gDebugMode != 0)
    {
        Texture2D<float4> debugMask = ResourceDescriptorHeap[gDebugMask];
        float4 d = debugMask.Sample(gsamPointClamp, vOut.TexC);
        color = lerp(color, d.rgb, 0.5f * d.a);
    }
    
    return float4(color, 1);
}