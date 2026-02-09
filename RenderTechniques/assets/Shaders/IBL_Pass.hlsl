SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

cbuffer cbPerFace : register(b0)
{
    float4x4 gMVP;
}

cbuffer cbPerPass : register(b0)
{
    float gRoughness;
    uint gEnvMapSize;
    uint gEnvMapLods;
    uint gTextureIdx;
}

struct VertexIn
{
    float3 PosL : POSITION;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosL : POSITION;
};

struct VertexOutPT
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

#include "IBL_Lighting.hlsli"

VertexOut VS(VertexIn vIn)
{
    VertexOut output;
    output.PosH = mul(float4(vIn.PosL, 1), gMVP);
    output.PosL = vIn.PosL;
    
    return output;
}

float4 PS_HDR2Cube(VertexOut vOut) : SV_Target
{
    float2 uv = SampleSphericalMap(normalize(vOut.PosL));
    
    Texture2D envMap2D = ResourceDescriptorHeap[gTextureIdx];
    float3 color = envMap2D.Sample(gsamPointWrap, float2(-uv.x, uv.y)).rgb;
    
    return float4(color, 1.0);
}

float4 PS_GenIrradianceMap(VertexOut vOut) : SV_Target
{
    float3 irradiance = 0;

    float3 normal = normalize(vOut.PosL);
    
    float3 up = float3(0.0, 1.0, 0.0);
    float3 right = normalize(cross(up, normal));
    up = normalize(cross(normal, right));

    float sampleDelta = 0.025;
    int nrSamples = 0;
    
    TextureCube envCubeMap = ResourceDescriptorHeap[gTextureIdx];
    
    for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
    {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
        {
            // spherical to cartesian (in tangent space)
            float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            // tangent space to world
            float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;

            irradiance += envCubeMap.Sample(gsamPointClamp, sampleVec).rgb * cos(theta) * sin(theta);
            nrSamples++;
        }
    }
    
    irradiance = PI * irradiance * (1.0 / float(nrSamples));
    
    return float4(irradiance, 1.0);
}

float4 PS_GenPrefilteredMap(VertexOut vOut) : SV_Target
{
    float3 N = normalize(vOut.PosL);
    float3 R = N;
    float3 V = R;

    const uint SAMPLE_COUNT = 1024u;
    float totalWeight = 0.0;
    float3 prefilteredColor = 0.0;
    
    TextureCube envCubeMap = ResourceDescriptorHeap[gTextureIdx];
    
    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        float2 Xi = Hammersley2D(i, SAMPLE_COUNT);
        float3 H = ImportanceSampleGGX(Xi, gRoughness, N);
        float3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0)
        {
            float AlphaRoughness = gRoughness * gRoughness;
            float pdf = max(SmithGGXSampleDirectionPDF(V, N, L, AlphaRoughness), 0.0001);
            float OmegaS = 1.0 / (float(SAMPLE_COUNT) * pdf);
            
            float OmegaP = ComputeCubeMapPixelSolidAngle(gEnvMapSize, gEnvMapSize);
            
            float MipBias = 1.0;
            float MipLevel = (AlphaRoughness == 0.0) ? 0.0 : clamp(0.5 * log2(OmegaS / max(OmegaP, 1e-10)) + MipBias, 0.0, gEnvMapLods - 1.0);
            
            prefilteredColor += envCubeMap.SampleLevel(gsamLinearWrap, L, MipLevel).rgb * NdotL;
            totalWeight += NdotL;
        }
    }
    
    prefilteredColor = prefilteredColor / totalWeight;

    return float4(prefilteredColor, 1.0);
}

float2 PS_GenBrdfLut(VertexOutPT vOut) : SV_Target
{
    float2 integratedBRDF = IntegrateBRDF(vOut.TexC.x, vOut.TexC.y, 512u);
    return integratedBRDF;
}