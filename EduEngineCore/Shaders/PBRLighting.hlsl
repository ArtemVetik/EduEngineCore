struct Light
{
    int Type;
    float Strength;
    float2 Padding;
    float3 Color;
    float FalloffStart; // point/spot light only
    float3 Direction;   // directional/spot light only
    float FalloffEnd;   // point/spot light only
    float3 Position;    // point light only
    float SpotPower;    // spot light only
};

#ifndef PBR_TEXTURED
#define PBR_TEXTURED 1
#endif

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

Texture2D gAlbedo : register(t0);
Texture2D gMetallicRoughness : register(t1);
Texture2D gAO : register(t2);
Texture2D gNormalMap : register(t3);
TextureCube gIrradianceMap : register(t4);
TextureCube gPrefilteredMap : register(t5);
Texture2D gBRDFLut : register(t6);
StructuredBuffer<Light> gLight : register(t7);

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
};

cbuffer cbPerPass : register(b1)
{
    float4x4 gViewProj;
    uint gDirectionalLightsCount;
    float3 gCamPos;
    uint gPrefilteredMapLods;
    uint3 gPadding;
}

cbuffer cbMaterial : register(b2)
{
    float4 gDiffuseAlbedo;
    float gRoughnessF;
    float gMetallicF;
    float gAOF;
    uint gPadding2;
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
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float3 TangentW : TANGENT;
    float2 TexC : TEXCOORD;
};

static const float PI = 3.14159265358979323846;

float3 fresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float3 fresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(1.0 - roughness, F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
	
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

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
    pin.NormalW = normalize(pin.NormalW);
    
#if PBR_TEXTURED
    float3 albedo = pow(gAlbedo.Sample(gsamLinearWrap, pin.TexC), 2.2) * gDiffuseAlbedo;
    float2 metallicRoughness = gMetallicRoughness.Sample(gsamLinearWrap, pin.TexC).gb;
    float ao = gAO.Sample(gsamLinearWrap, pin.TexC).r;
    
    float roughness = metallicRoughness.r;
    float metallic = metallicRoughness.g;
    
    float3 normalMapSample = gNormalMap.Sample(gsamAnisotropicWrap, pin.TexC).xyz;
    float3 N = NormalSampleToWorldSpace(normalMapSample, pin.NormalW, pin.TangentW);
#else
    float3 albedo = gDiffuseAlbedo;
    float roughness = gRoughnessF;
    float metallic = gMetallicF;
    float ao = gAOF;
    float3 N = pin.NormalW;
#endif

    float3 F0 = 0.04;
    F0 = lerp(F0, albedo, metallic);
    
    float3 V = normalize(gCamPos - pin.PosW);

    // reflectance equation
    float3 Lo = 0.0;
    
    for (int i = 0; i < gDirectionalLightsCount; ++i)
    {
        // calculate per-light radiance
        float3 L = normalize(-gLight[i].Direction);
        float3 H = normalize(V + L);
        
        float3 radiance = gLight[i].Color * gLight[i].Strength;
        
        // cook-torrance brdf
        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        float3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        
        float3 kS = F;
        float3 kD = 1.0 - kS;
        kD *= 1.0 - metallic;
        
        float3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        float3 specular = numerator / denominator;
            
        // add to outgoing radiance Lo
        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }
    
    float3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);

    float3 kS = F;
    float3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;
    
    float3 irradiance = gIrradianceMap.Sample(gsamLinearWrap, N).rgb;
    float3 diffuse = irradiance * albedo;
    
    float3 R = reflect(-V, N);
    float3 prefilteredColor = gPrefilteredMap.SampleLevel(gsamLinearWrap, R, roughness * gPrefilteredMapLods).rgb;
    
    float2 envBRDF = gBRDFLut.Sample(gsamLinearClamp, float2(max(dot(N, V), 0.0), roughness)).rg;
    float3 specular = prefilteredColor * (F * envBRDF.x + envBRDF.y);
    float3 ambient = (kD * diffuse + specular) * ao;
    
    float3 color = ambient + Lo;
	
    color = color / (color + 1.0);
    color = pow(color, 1.0 / 2.2);
    
    return float4(color, 1.0);
}