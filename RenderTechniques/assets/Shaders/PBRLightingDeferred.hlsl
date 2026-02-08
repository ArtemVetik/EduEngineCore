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

#define MAX_CASCADES 4

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);

StructuredBuffer<Light> gLight : register(t7);

cbuffer cbPass : register(b1)
{
    float4x4 gViewInv;
    float4x4 gProjInv;
    uint gDirectionalLightsCount;
    float3 gCamPos;
    uint gPrefilteredMapLods;
    uint gCascadeCount;
    uint2 gPadding1;
    float4x4 gCascadeTransform[MAX_CASCADES];
    float4 gCascadeShadowSphere[MAX_CASCADES];
    float4 gCascadeShadowRad2;
    float4 gCascadeDistance;
}

cbuffer cbTextureIndexes : register(b2)
{
    uint gAlbedoTexIdx;
    uint gNormalMapIdx;
    uint gMetallicRoughAoIdx;
    uint gDepthBufferIdx;
    uint gSsaoMapIdx;
    uint gIrradianceMapIdx;
    uint gPrefilteredMapIdx;
    uint gBRDFLutIdx;
    uint4 gShadowMapIdx;
}

cbuffer cbMaterial : register(b3)
{
    float4 gDiffuseAlbedo;
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
    float2 TexC : TEXCOORD;
};

#include "PackNormals.hlsl"

static const float PI = 3.14159265358979323846;

float3 fresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float3 fresnelSchlickRoughness(float VdotH, float3 F0, float roughness)
{
    return F0 + (max(1.0 - roughness, F0) - F0) * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);
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
// PCF for shadow mapping.
//---------------------------------------------------------------------------------------

float CalcShadowFactor(float4 shadowPosH, uint mapIndex)
{
    Texture2D shadowMap = ResourceDescriptorHeap[gShadowMapIdx[mapIndex]];
    
    float depth = shadowPosH.z;

    uint width, height, numMips;
    shadowMap.GetDimensions(0, width, height, numMips);
	
    float dx = 1.0f / (float) width;
    dx = 0.5 * dx;
    
    float percentLit = 0.0f;
    const float2 offsets[9] =
    {
        float2(-dx, -dx), float2(0.0f, -dx), float2(dx, -dx),
        float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
        float2(-dx, +dx), float2(0.0f, +dx), float2(dx, +dx)
    };
	
    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        percentLit += shadowMap.SampleCmpLevelZero(gsamShadow, shadowPosH.xy + offsets[i], depth).r;
    }
	
    return percentLit / 9.0f;
}

uint ComputeCascadeIndex(float3 positionWS)
{
    float3 fromCenter0 = positionWS - gCascadeShadowSphere[0].xyz;
    float3 fromCenter1 = positionWS - gCascadeShadowSphere[1].xyz;
    float3 fromCenter2 = positionWS - gCascadeShadowSphere[2].xyz;
    float3 fromCenter3 = positionWS - gCascadeShadowSphere[3].xyz;
    float4 distances2 = float4(dot(fromCenter0, fromCenter0), dot(fromCenter1, fromCenter1), dot(fromCenter2, fromCenter2), dot(fromCenter3, fromCenter3));

    float4 weights = float4(distances2 < gCascadeShadowRad2);
    weights.yzw = saturate(weights.yzw - weights.xyz);

    return 4 - dot(weights, float4(4, 3, 2, 1));
}

float4 PS(VertexOut pin) : SV_Target
{
    Texture2D albedoTex = ResourceDescriptorHeap[gAlbedoTexIdx];
    Texture2D normalWTex = ResourceDescriptorHeap[gNormalMapIdx];
    Texture2D metallicRoughAoTex = ResourceDescriptorHeap[gMetallicRoughAoIdx];
    
    float3 albedo = pow(albedoTex.Sample(gsamPointWrap, pin.TexC), 2.2).xyz * gDiffuseAlbedo.xyz;
    
    float3 metallicRoughAo = metallicRoughAoTex.Sample(gsamPointWrap, pin.TexC).rgb;
    
    float metallic = metallicRoughAo.r;
    float roughness = metallicRoughAo.g;
    float ao = metallicRoughAo.b;
    
#if PACK_NORMALS > 0
    half2 packedN = normalWTex.Sample(gsamPointWrap, pin.TexC).xy;
    half3 N = normal_decode(packedN);
#else
    half3 N = normalWTex.Sample(gsamPointWrap, pin.TexC).xyz;
#endif
    
    float3 F0 = 0.04;
    F0 = lerp(F0, albedo, metallic);
    
    Texture2D depthBuffer = ResourceDescriptorHeap[gDepthBufferIdx];
    float depth = depthBuffer.Sample(gsamPointClamp, pin.TexC).r;
    
    float4 clipSpacePosition = float4(pin.TexC * 2 - 1, depth, 1);
    clipSpacePosition.y *= -1.0f;
    float4 viewSpacePosition = mul(gProjInv, clipSpacePosition);
    viewSpacePosition /= viewSpacePosition.w;
    float4 worldSpacePosition = mul(gViewInv, viewSpacePosition);
	
    float4 posW = worldSpacePosition;
    
    uint cascadeIndex = ComputeCascadeIndex(posW.xyz);
    
    float shadowFactor = 1.0f;
    
    if (cascadeIndex < gCascadeCount)
    {
        float4 shadowPosH = mul(float4(posW.xyz, 1), gCascadeTransform[cascadeIndex]);
        shadowFactor = CalcShadowFactor(shadowPosH, cascadeIndex);
    }
    
    float3 V = normalize(gCamPos - posW.rgb);

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
        Lo += shadowFactor * (kD * albedo / PI + specular) * radiance * NdotL;
    }
    
    float3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    
    float3 kS = F;
    float3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;
    
    TextureCube irradianceMapTex = ResourceDescriptorHeap[gIrradianceMapIdx];
    TextureCube prefilteredMapTex = ResourceDescriptorHeap[gPrefilteredMapIdx];
    Texture2D brdfLutTex = ResourceDescriptorHeap[gBRDFLutIdx];
    Texture2D ssaoMap = ResourceDescriptorHeap[gSsaoMapIdx];
    
    float ssao = ssaoMap.Sample(gsamPointWrap, pin.TexC).r;
    
    float3 irradiance = irradianceMapTex.Sample(gsamLinearWrap, N).rgb;
    float3 diffuse = 0.2f * irradiance * albedo; // TODO: temporary multiply by 0.2f
    
    float3 R = reflect(-V, N);
    float3 prefilteredColor = prefilteredMapTex.SampleLevel(gsamLinearWrap, R, roughness * gPrefilteredMapLods).rgb;
    
    float2 envBRDF = brdfLutTex.Sample(gsamLinearClamp, float2(max(dot(N, V), 0.0), roughness)).rg;
    float3 specular = prefilteredColor * (F * envBRDF.x + envBRDF.y);
    float3 ambient = (kD * diffuse + specular) * ao * ssao;
    
    float3 color = ambient + Lo;
	
#if DEBUGVIEW_ROUGHNESS
    color = roughness;
#elif DEBUGVIEW_METALLIC
    color = metallic;
#elif DEBUGVIEW_AO
    color = ao;
#elif DEBUGVIEW_NORMAL
    color = N;
#elif DEBUGVIEW_DIFFUSE_IBL
    color = kD * diffuse;
#elif DEBUGVIEW_SPECULAR_IBL
    color = specular;
#elif DEBUGVIEW_NDOTV
    color = max(dot(N, V), 0.0);
#elif DEBUGVIEW_FRESNEL
    color = F;
#elif DEBUGVIEW_BRDF_Y
    color = envBRDF.y;
#elif DEBUGVIEW_BRDF_X
    color = envBRDF.x;
#elif DEBUGVIEW_SSAO
    color = ssao;
#elif DEBUGVIEW_SHADOWS
    color = shadowFactor;
#endif
    
    return float4(color, 1.0);
}