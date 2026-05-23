SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

cbuffer cbPass : register(b0)
{
    float4x4 gWorld;
    float4x4 gViewProj;
    float3 gCameraPos;
    uint gPadding0;
    float3 gMainLightPos;
    uint gPadding1;
    float3 gMainLightColor;
};

cbuffer cbConstants : register(b1)
{
    uint gMaxLodLevel;
    uint gTesselationLevel;
    float gMaxTesselationDistance;
    float gTesselationDecayFactor;
    float gCullingTollerance;

    uint gNbCascades;
    uint gWavelengthsIdx;           // StructuredBuffer<float>
    uint gDisplacementsTexturesIdx; // Texture2DArray<float3>
    uint gDerivativesTexturesIdx;   // Texture2DArray<float4>
    uint gTurbulenceTexturesIdx;    // Texture2DArray<float4>
    uint gReflectionCube;           // TextureCube
    
    float gEnvironmentReflectionStrength;
    float3 gSubsurfaceScatteringColor;
    float gSubsurfaceScatteringIntensity;
    float3 gColor;
    float gWaterFogDensity;
    float gRefractionStrength;
    float gRoughness;
    float gEX;
    float gEY;
    float gFoamBlending;
    float gFoamThreshold;
    float2 gFoamPadRow;
    float3 gFoamColor;
    float gFoamPad0;
    float3 gShadowsColor;
    float gShadowsIntensity;
    float gSunReflectionStrength;
    float3 gDrawConstantsPad1;
    float4 gDrawConstantsPad2[5];
}

#include "..\CommonTransforms.hlsli"

struct VertexInput
{
    float3 PosL : POSITION;
};

struct TessellationControlPoint
{
    float3 PositionWS : INTERNALTESSPOS;
    float4 PositionCS : SV_POSITION;
};

struct TessellationFactors
{
    float Edge[3] : SV_TessFactor;
    float Inside  : SV_InsideTessFactor;
};

struct Domain2FragmentData
{
    float4 PositionCS : SV_POSITION;
    float3 ViewDir    : TEXCOORD0;
    float3 PositionWS : TEXCOORD1;
    float2 WorldUV    : TEXCOORD2;
    float2 PositionSS : TEXCOORD3;
    half LodLevel     : TEXCOORD4;
};

struct VertexOutput
{
    float4 PosH : SV_POSITION;
};

TessellationControlPoint VS(VertexInput input)
{
    TessellationControlPoint output;
    output.PositionWS = mul(float4(input.PosL, 1.0f), gWorld).xyz;
    output.PositionCS = mul(float4(output.PositionWS, 1.0f), gViewProj);
    return output;
}

#define M_PI 3.1415926535897932384626433832795f
#define FLT_MIN 1.175494351e-38
#define WATER_REFRACTION_INDEX 1.333f
#define AIR_REFRACTION_INDEX 1.0f
#define R0 pow((AIR_REFRACTION_INDEX - WATER_REFRACTION_INDEX) / (AIR_REFRACTION_INDEX + WATER_REFRACTION_INDEX), 2)

// Returns the color of what is rendered behind, distorts it to simulate refraction and blends with an adjustable color to cheaply apporoximate underwater light absorption.
// Based on https://catlikecoding.com/unity/tutorials/flow/looking-through-water/
float3 UnderwaterView(float2 positionSS, float3 normalWS)
{
    // TODO: Implement depth-based fog
    return gColor;
}

// Returns a fast subsurface scattering approximation based on the height of the wave, the light direction and the view direction.
float3 SubsurfaceScatteringApproximation(float waveHeight, float3 lightDir, float3 viewDir)
{
    float coeff = gSubsurfaceScatteringIntensity * max(0, waveHeight) * pow(max(0, dot(lightDir, viewDir)), 4);
    return coeff * gSubsurfaceScatteringColor * gMainLightColor;
}

// Returns the reflection of the sky color by sampling the skybox cubemap.
float3 EnvironmentReflections(float3 viewDir, float3 normalWS)
{
    TextureCube reflectionCube = ResourceDescriptorHeap[gReflectionCube];
    
    float3 reflectionDir = -reflect(viewDir, float3(0.0, 1.0, 0.0));
    //float3 reflectionDir = -reflect(viewDir, normalWS);
    //float3 reflectionDir = normalWS;
    
    float3 environment = reflectionCube.Sample(gsamAnisotropicWrap, reflectionDir).xyz;
    return environment * gEnvironmentReflectionStrength;
}

// Code source: https://rtarun9.github.io/blogs/physically_based_rendering/#what-is-physically-based-rendering
float NormalDistribution(float3 h, float3 normalWS, float3 viewDir, float roughness)
{
    float alpha = roughness * roughness;
    float alphaSquare = alpha * alpha;
    float nDotH = saturate(dot(normalWS, h));
    
    return alphaSquare / (max(M_PI * pow((nDotH * nDotH * (alphaSquare - 1.0f) + 1.0f), 2.0f), FLT_MIN));
}

// Code source: https://rtarun9.github.io/blogs/physically_based_rendering/#what-is-physically-based-rendering
float SchlickBeckmannGS(float3 normalWS, float3 x, float roughness)
{
    float k = roughness / 2.0f;
    float nDotX = saturate(dot(normalWS, x));
                
    return nDotX / (max((nDotX * (1.0f - k) + k), FLT_MIN));
}

// Code source: https://rtarun9.github.io/blogs/physically_based_rendering/#what-is-physically-based-rendering
float GeometryShadowingFunction(float3 normalWS, float3 viewDir, float3 lightDir, float roughness)
{
    return SchlickBeckmannGS(normalWS, viewDir, roughness) * SchlickBeckmannGS(normalWS, lightDir, roughness);
}

// Computes the Cook-Torrance BRDF model for specular reflection
// Code source: https://rtarun9.github.io/blogs/physically_based_rendering/#what-is-physically-based-rendering
float3 CookTorranceBRDF(float3 h, float3 normalWS, float3 viewDir, float3 lightDir, float fresnel, float roughness)
{
    if (dot(lightDir, float3(0.0, 1.0, 0.0)) <= 0.0)
        return 0.0;
    float normalDistribution = max(NormalDistribution(h, normalWS, viewDir, roughness), 0.0);
    float geometryFunction = max(GeometryShadowingFunction(normalWS, viewDir, lightDir, roughness), 0.0);

    return gMainLightColor * normalDistribution * geometryFunction / max(8.0f * saturate(dot(viewDir, normalWS)) * saturate(dot(lightDir, normalWS)), FLT_MIN);
}

// Computes the Ashikhmin-Shirley anisotropic BRDF model for specular reflection.
// https://www.researchgate.net/publication/2523875_An_anisotropic_phong_BRDF_model
float3 AshikhminShirleyBRDF(float3 h, float3 viewDir, float3 lightDir, float3 normalWS, float fresnel, float ex, float ey)
{
    if (dot(lightDir, float3(0.0, 1.0, 0.0)) <= 0.0)
        return 0.0;
    
    float cos2PhiH = max((h.x * h.x) / max(1.0 - h.z * h.z, FLT_MIN), 0.0);
    float sin2PhiH = max((h.y * h.y) / max(1.0 - h.z * h.z, FLT_MIN), 0.0);
    float d = sqrt((ex + 1) * (ey + 1)) * pow(max(dot(h, normalWS), 0.0), ex * cos2PhiH + ey * sin2PhiH);

    return gMainLightColor * max(d * fresnel / max(8 * M_PI * dot(h, viewDir) * max(dot(normalWS, viewDir), dot(normalWS, lightDir)), FLT_MIN), 0.0);
}

// Returns true if the point is outside the bounds set by lower and higher.
// Code source: https://nedmakesgames.medium.com/mastering-tessellation-shaders-and-their-many-uses-in-unity-9caeb760150e
bool IsOutOfBounds(float3 p, float3 lower, float3 higher)
{
    return p.x < lower.x || p.x > higher.x || p.y < lower.y || p.y > higher.y || p.z < lower.z || p.z > higher.z;
}

// Returns true if the given vertex is outside the camera fustum and should be culled.
// Code source: https://nedmakesgames.medium.com/mastering-tessellation-shaders-and-their-many-uses-in-unity-9caeb760150e
bool IsPointOutOfFrustum(float4 PositionCS)
{
    float3 culling = PositionCS.xyz;
    float w = PositionCS.w;
    
    // Clip space frustum is defined by the bounds:
    // -w <= x <= w
    // -w <= y <= w
    //  0 <= z <= w
    //
    // We add a culling tolerance to avoid popping when vertices are close to the edge of the frustum.
    float3 lowerBounds = float3(-w - gCullingTollerance, -w - gCullingTollerance, -gCullingTollerance);
    float3 higherBounds = float3(w + gCullingTollerance, w + gCullingTollerance, w + gCullingTollerance);
    return IsOutOfBounds(culling, lowerBounds, higherBounds);
}

// Returns true if it should be clipped due to frustum or winding culling.
// Code source: https://nedmakesgames.medium.com/mastering-tessellation-shaders-and-their-many-uses-in-unity-9caeb760150e
bool ShouldClipPatch(float4 p0PositionCS, float4 p1PositionCS, float4 p2PositionCS)
{
    bool allOutside = IsPointOutOfFrustum(p0PositionCS) &&
                    IsPointOutOfFrustum(p1PositionCS) &&
                    IsPointOutOfFrustum(p2PositionCS);
    return allOutside;
}

// Returns the tessellation factor based on the distance between a given world space position and the camera.
// It decreases exponentially as the distance increases.
float DistanceBasedTessFactor(float3 PositionWS, float minDist, float maxDist, float tess)
{
    float dist = distance(PositionWS.xyz, gCameraPos);
    float normalizedDist = saturate((dist - minDist) / (maxDist - minDist));
    float decayFactor = exp(-gTesselationDecayFactor * normalizedDist);
    float f = saturate(decayFactor) * tess;
    return f;
}

TessellationFactors PatchConstantFunction(InputPatch<TessellationControlPoint, 3> patch)
{
    TessellationFactors f;
    if (ShouldClipPatch(patch[0].PositionCS, patch[1].PositionCS, patch[2].PositionCS))
    {
        f.Edge[0] = f.Edge[1] = f.Edge[2] = f.Inside = 0; // Cull the patch
    }
    else
    {
        float3 EdgePosition0 = 0.5 * (patch[1].PositionWS + patch[2].PositionWS);
        float3 EdgePosition1 = 0.5 * (patch[0].PositionWS + patch[2].PositionWS);
        float3 EdgePosition2 = 0.5 * (patch[0].PositionWS + patch[1].PositionWS);

        f.Edge[0] = DistanceBasedTessFactor(EdgePosition0, 1, gMaxTesselationDistance, gTesselationLevel);
        f.Edge[1] = DistanceBasedTessFactor(EdgePosition1, 1, gMaxTesselationDistance, gTesselationLevel);
        f.Edge[2] = DistanceBasedTessFactor(EdgePosition2, 1, gMaxTesselationDistance, gTesselationLevel);
        f.Inside = (f.Edge[0] + f.Edge[1] + f.Edge[2]) / 3.0;
    }
    return f;
}

[domain("tri")]
[outputcontrolpoints(3)]
[outputtopology("triangle_cw")]
[patchconstantfunc("PatchConstantFunction")]
[partitioning("integer")]
TessellationControlPoint Hull(InputPatch<TessellationControlPoint, 3> patch, uint id : SV_OutputControlPointID)
{
    return patch[id];
}

[domain("tri")]
Domain2FragmentData Domain(TessellationFactors factors, OutputPatch<TessellationControlPoint, 3> patch, float3 barycentricCoordinates : SV_DomainLocation)
{
    Texture2DArray<float3> displacementsTextures = ResourceDescriptorHeap[gDisplacementsTexturesIdx];
    StructuredBuffer<float> wavelengths = ResourceDescriptorHeap[gWavelengthsIdx];
    
    Domain2FragmentData output;
    output.PositionWS = patch[0].PositionWS * barycentricCoordinates.x +
                        patch[1].PositionWS * barycentricCoordinates.y +
                        patch[2].PositionWS * barycentricCoordinates.z;
    
    output.WorldUV = output.PositionWS.xz;

    float lodFactor = distance(output.PositionWS, gCameraPos) / gMaxTesselationDistance;
    output.LodLevel = lerp(0.0, gMaxLodLevel, lodFactor);

    float3 displacement = 0;
    for (uint i = 0; i < gNbCascades; i++)
    {
        displacement += displacementsTextures.SampleLevel(gsamAnisotropicWrap, float3(output.WorldUV / wavelengths[i], i), output.LodLevel);
    }
    
    output.PositionWS += displacement;

    output.PositionCS = mul(float4(output.PositionWS, 1.0f), gViewProj);
    output.ViewDir = normalize(gCameraPos - output.PositionWS);
    
    float2 uv = output.PositionCS.xy / output.PositionCS.w;
    uv = uv * 0.5f + 0.5f;
    output.PositionSS = uv;

    return output;
}

float4 PS(Domain2FragmentData input) : SV_TARGET
{
    Texture2DArray<float4> derivativesTextures = ResourceDescriptorHeap[gDerivativesTexturesIdx];
    Texture2DArray<float4> turbulenceTextures = ResourceDescriptorHeap[gTurbulenceTexturesIdx];
    StructuredBuffer<float> wavelengths = ResourceDescriptorHeap[gWavelengthsIdx];
    
    float4 derivatives = 0;
    float turbulence = 0;
    
    //[unroll]
    for (int i = 0; i < gNbCascades; i++)
    {
        float2 uv = input.WorldUV / wavelengths[i];
        
        derivatives += derivativesTextures.SampleLevel(gsamAnisotropicWrap, float3(uv, i), input.LodLevel);
        turbulence += 1 - saturate(turbulenceTextures.SampleLevel(gsamAnisotropicWrap, float3(uv, i), input.LodLevel).x);
    }

    float2 slope = float2(derivatives.x / (1 + derivatives.z), derivatives.y / (1 + derivatives.w));
    float3 normalOS = normalize(float3(-slope.x, 1, -slope.y));
    float3 normalWS = normalize(TransformNormalToWorldSpace(normalOS, gWorld));

    float3 lightDir = normalize(gMainLightPos);
    float3 halfwayVec = normalize(input.ViewDir + lightDir);

    float fresnel = R0 + (1 - R0) * pow(1.0 - saturate(dot(normalWS, input.ViewDir)), 5 * exp(-2.69 * gRoughness)) / (1 + 22.7 * pow(gRoughness, 1.5));
    float fresnelH = R0 + (1 - R0) * pow(1.0 - saturate(dot(halfwayVec, input.ViewDir)), 5);
    
    float shadowFactor = 1; // TODO: Sample Shadow Map

    half3 refraction = UnderwaterView(input.PositionSS, normalWS);
    refraction += SubsurfaceScatteringApproximation(input.PositionWS.y, lightDir, -input.ViewDir);

    half3 reflections = EnvironmentReflections(input.ViewDir, normalWS);
    refraction *= reflections;
    float nu = gEX * 10.0 * (1.0 - gRoughness); // Controls anisotropy along x-axis
    float nv = gEY * 10.0 * (1.0 - gRoughness); // Controls anisotropy along z-axis
    half3 ashikhminShirleySpec = AshikhminShirleyBRDF(halfwayVec, input.ViewDir, lightDir, normalWS, fresnelH, nu, nv);
    half3 cookTorranceSpec = CookTorranceBRDF(halfwayVec, normalWS, input.ViewDir, lightDir, fresnelH, gRoughness);
    // Blending factor based on view angle, adding Ashikhmin-Shirley at flatter angles
    reflections += (cookTorranceSpec + ashikhminShirleySpec * saturate(dot(input.ViewDir, normalWS))) * shadowFactor * gSunReflectionStrength;

    float3 emission = lerp(lerp(refraction, reflections, fresnel), gShadowsColor, gShadowsIntensity * (1 - shadowFactor));
    if (turbulence >= gFoamThreshold)
        emission = lerp(emission, gFoamColor, gFoamBlending);
    
    return float4(emission, 1.0f);
}