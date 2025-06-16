#include "LightingUtil.hlsl"

SamplerState gAlbedo_sampler : register(s0);

Texture2D gAlbedo : register(t0);
StructuredBuffer<Light> gLight : register(t1);

cbuffer cbPerObject : register(b0)
{
	float4x4 gWorld; 
};

cbuffer cbPerPass : register(b1)
{
    float4x4 gViewProj;
    float4 gAmbientLight;
    uint gDirectionalLightsCount;
    float3 gCamPos;
}

cbuffer cbMaterial : register(b2)
{
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float gRoughness;
}

struct VertexIn
{
    float3 PosL     : POSITION;
    float3 NormalL  : NORMAL;
    float3 TangentU : TANGENT;
    float2 TexC     : TEXCOORD;
};

struct VertexOut
{
	float4 PosH  : SV_POSITION0;
	float3 PosW  : POSITION;
	float3 NormalW  : NORMAL;
    float3 TangentW : TANGENT;
    float2 TexC : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut) 0.0f;

    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;
    vout.PosH = mul(posW, gViewProj);

    vout.NormalW = mul(vin.NormalL, (float3x3) gWorld);
    vout.TangentW = mul(vin.TangentU, (float3x3) gWorld);
    vout.TexC = vin.TexC;
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    float4 diffuseAlbedo = gAlbedo.Sample(gAlbedo_sampler, float2(pin.TexC.x, pin.TexC.y)) * gDiffuseAlbedo;
    float4 ambient = diffuseAlbedo * gAmbientLight;
    
    pin.NormalW = normalize(pin.NormalW);
    
    Material mat;
    mat.DiffuseAlbedo = diffuseAlbedo;
    mat.FresnelR0 = gFresnelR0;
    mat.Shininess = 1 - gRoughness;
    
    float3 light = 0.0f;
    float3 eyePosW = normalize(gCamPos - pin.PosW);
    light += ComputeDirectionalLight(gLight[0], mat, pin.NormalW, eyePosW);
    
    float4 litColor = ambient + float4(light, 0);
    litColor.a = ambient.a;
    
    return litColor;
}


