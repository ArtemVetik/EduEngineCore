SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

cbuffer cbPass : register(b0)
{
    float4x4 gViewProj;
    float3 gCameraPos;
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
}

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
    float4 PositionSS : TEXCOORD3;
    half LodLevel     : TEXCOORD4;
};

struct VertexOutput
{
    float4 PosH : SV_POSITION;
};

TessellationControlPoint VS(VertexInput input)
{
    TessellationControlPoint output;
    output.PositionWS = input.PosL;
    output.PositionCS = mul(float4(input.PosL, 1.0f), gViewProj);
    return output;
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

[domain("tri")] // Signal we're inputting triangles
[outputcontrolpoints(3)] // Triangles have three points
[outputtopology("triangle_cw")] // Signal we're outputting triangles
[patchconstantfunc("PatchConstantFunction")] // Register the patch constant function
[partitioning("integer")] // Select a partitioning mode: integer, fractional_odd, fractional_even or pow2
TessellationControlPoint Hull(InputPatch<TessellationControlPoint, 3> patch, uint id : SV_OutputControlPointID)
{
    return patch[id];
}

[domain("tri")] // Signal we're inputting triangles
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
        displacement += displacementsTextures.SampleLevel(gsamLinearWrap, float3(output.WorldUV / wavelengths[i], i), output.LodLevel);
    }
    
    output.PositionWS += displacement;

    output.PositionCS = mul(float4(output.PositionWS, 1.0f), gViewProj);
    output.ViewDir = normalize(gCameraPos - output.PositionWS);

    return output;
}

float4 PS(Domain2FragmentData input) : SV_TARGET
{
    return float4(0.0f, 0.5f, 1.0f, 1.0f);
}