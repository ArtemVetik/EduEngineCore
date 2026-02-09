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

#include "PackNormals.hlsli"
#include "CommonTransforms.hlsli"
#include "PBR_Lighting.hlsli"
#include "Shadows.hlsli"
#include "ReflectionProbe.hlsli"

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
    uint gReflectionProbesCount;
    uint gReflectionProbesBuffIdx;
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

float CalcShadowFactor(float3 posW)
{
    float shadowFactor = 1.0f;
    uint cascadeIndex = ComputeCascadeIndex(posW, gCascadeShadowSphere, gCascadeShadowRad2);
    
    if (cascadeIndex < gCascadeCount)
    {
        float4 shadowPosH = mul(float4(posW, 1), gCascadeTransform[cascadeIndex]);
        
        Texture2D shadowMap = ResourceDescriptorHeap[gShadowMapIdx[cascadeIndex]];
        shadowFactor = CalcShadowFactor(shadowMap, gsamShadow, shadowPosH);
    }
    
    return shadowFactor;
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
    N = normalize(N);
    
    Texture2D depthBuffer = ResourceDescriptorHeap[gDepthBufferIdx];
    float depth = depthBuffer.Sample(gsamPointClamp, pin.TexC).r;
    
    float4 posW = ReconstructWorldPosFromDepth(depth, pin.TexC, gProjInv, gViewInv);
    float shadowFactor = CalcShadowFactor(posW.xyz);
    
    float3 F0 = 0.04;
    F0 = lerp(F0, albedo, metallic);
    
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
    
    float3 R = reflect(-V, N);
    
    StructuredBuffer<ReflectionProbe> reflectionProbes = ResourceDescriptorHeap[gReflectionProbesBuffIdx];
    
    float3 irradianceColor;
    float3 prefilteredColor;
    CalculateProbesIrradianceAndReflection(reflectionProbes,
                                           gReflectionProbesCount,
                                           gsamLinearWrap,
                                           posW.xyz,
                                           R,
                                           N,
                                           roughness * gPrefilteredMapLods,
                                           gIrradianceMapIdx,
                                           gPrefilteredMapIdx,
                                           irradianceColor,
                                           prefilteredColor);
    
    float3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    
    float3 kS = F;
    float3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;
    
    Texture2D brdfLutTex = ResourceDescriptorHeap[gBRDFLutIdx];
    Texture2D ssaoMap = ResourceDescriptorHeap[gSsaoMapIdx];
    
    float ssao = ssaoMap.Sample(gsamPointWrap, pin.TexC).r;
    
    float3 diffuse = irradianceColor * albedo;
    
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