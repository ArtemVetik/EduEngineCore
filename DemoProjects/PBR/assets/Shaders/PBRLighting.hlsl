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

StructuredBuffer<Light> gLight : register(t7);

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    
    uint gAlbedoTexIdx;
    uint gNormalMapIdx;
    uint gORMTexIdx;
};

cbuffer cbPerPass : register(b1)
{
    float4x4 gViewProj;
    uint gDirectionalLightsCount;
    float3 gCamPos;
    uint gPrefilteredMapLods;
    uint3 gPadding1;
}

cbuffer cbTextureIndexes : register(b2)
{
    uint gIrradianceMapIdx;
    uint gPrefilteredMapIdx;
    uint gBRDFLutIdx;
    uint gPadding2;
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

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut) 0.0f;

    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;
    vout.PosH = mul(posW, gViewProj);

    float3x3 NormalTransform = float3x3(gWorld[0].xyz, gWorld[1].xyz, gWorld[2].xyz);
    NormalTransform = InverseTranspose3x3(NormalTransform);
    
    vout.NormalW = normalize(mul(vin.NormalL, NormalTransform));
    
    vout.TangentW = normalize(mul(vin.TangentU, float3x3(gWorld[0].xyz, gWorld[1].xyz, gWorld[2].xyz)));
    vout.TexC = vin.TexC;
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    pin.NormalW = normalize(pin.NormalW);
    
    float3 albedo = float3(0.0, 0.0, 0);
    
    if (gAlbedoTexIdx != -1)
    {
        Texture2D albedoTex = ResourceDescriptorHeap[gAlbedoTexIdx];
        albedo = pow(albedoTex.Sample(gsamLinearWrap, pin.TexC), 2.2) * gDiffuseAlbedo;
    }
    
    float3 N = pin.NormalW;
    if (gNormalMapIdx != -1)
    {
        Texture2D normalMapTex = ResourceDescriptorHeap[gNormalMapIdx];
        float3 normalMapSample = normalMapTex.Sample(gsamAnisotropicWrap, pin.TexC).xyz;
        N = NormalSampleToWorldSpace(normalMapSample, pin.NormalW, pin.TangentW);
    }
    
    float3 orm = float3(1.0f, 0.0, 0.0);
    if (gORMTexIdx != -1)
    {
        Texture2D ormTex = ResourceDescriptorHeap[gORMTexIdx];
        orm = ormTex.Sample(gsamLinearWrap, pin.TexC).rgb;
    }
    
    float ao = orm.r;
    float roughness = orm.g;
    float metallic = orm.b;
    
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
    
    TextureCube irradianceMapTex = ResourceDescriptorHeap[gIrradianceMapIdx];
    TextureCube prefilteredMapTex = ResourceDescriptorHeap[gPrefilteredMapIdx];
    Texture2D brdfLutTex = ResourceDescriptorHeap[gBRDFLutIdx];
    
    float3 irradiance = irradianceMapTex.Sample(gsamLinearWrap, N).rgb;
    float3 diffuse = irradiance * albedo;
    
    float3 R = reflect(-V, N);
    float3 prefilteredColor = prefilteredMapTex.SampleLevel(gsamLinearWrap, R, roughness * gPrefilteredMapLods).rgb;
    
    float2 envBRDF = brdfLutTex.Sample(gsamLinearClamp, float2(max(dot(N, V), 0.0), roughness)).rg;
    float3 specular = prefilteredColor * (F * envBRDF.x + envBRDF.y);
    float3 ambient = (kD * diffuse + specular) * ao;
    
    float3 color = ambient + Lo;
	
    color = color / (color + 1.0);
    
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
#endif
    
    color = pow(color, 1.0 / 2.2);
    
    return float4(color, 1.0);
}