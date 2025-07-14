struct Light
{
    int Type;
    float3 Padding;
    float3 Strength;
    float FalloffStart; // point/spot light only
    float3 Direction;   // directional/spot light only
    float FalloffEnd;   // point/spot light only
    float3 Position;    // point light only
    float SpotPower;    // spot light only
};

SamplerState gAlbedo_sampler : register(s0);
SamplerState gMetallicRoughness_sampler : register(s1);
SamplerState gAO_sampler : register(s2);
SamplerState gNormalMap_sampler : register(s3);

Texture2D gAlbedo : register(t0);
Texture2D gMetallicRoughness : register(t1);
Texture2D gAO : register(t2);
Texture2D gNormalMap : register(t3);
StructuredBuffer<Light> gLight : register(t4);

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
};

cbuffer cbPerPass : register(b1)
{
    float4x4 gViewProj;
    uint gDirectionalLightsCount;
    float3 gCamPos;
}

cbuffer cbMaterial : register(b2)
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

static const float PI = 3.14159265359;

float3 fresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
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
    pin.TexC.y = -pin.TexC.y;
    float3 albedo = pow(gAlbedo.Sample(gAlbedo_sampler, pin.TexC), 2.2) * gDiffuseAlbedo;
    float2 metallicRoughness = gMetallicRoughness.Sample(gMetallicRoughness_sampler, pin.TexC).gb;
    float ao = gAO.Sample(gAO_sampler, pin.TexC).r;
    
    float roughness = metallicRoughness.x;
    float metallic = metallicRoughness.y;
    
    pin.NormalW = normalize(pin.NormalW);
    float3 V = normalize(gCamPos - pin.PosW);
    
    float3 normalMapSample = gNormalMap.Sample(gNormalMap_sampler, pin.TexC);
    float3 N = NormalSampleToWorldSpace(normalMapSample, pin.NormalW, pin.TangentW);
    
    float3 F0 = 0.04;
    F0 = lerp(F0, albedo, metallic);
	           
    // reflectance equation
    float3 Lo = 0.0;
    for (int i = 0; i < 1; ++i)
    {
        // calculate per-light radiance
        float3 L = normalize(gLight[i].Position - pin.PosW);
        float3 H = normalize(V + L);
        
        float3 radiance = gLight[i].Strength;
        
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
  
    float3 ambient = 0.03 * albedo * ao;
    float3 color = ambient + Lo;
	
    color = color / (color + 1.0);
    color = pow(color, 1.0 / 2.2);
   
    return float4(color, 1.0);
}