static const float PI = 3.14159265358979323846;

float2 SampleSphericalMap(float3 v)
{
    float2 uv = float2(atan2(v.z, v.x), asin(v.y));
    uv *= float2(0.1591, 0.3183);
    uv += 0.5;
    return uv;
}

float2 Hammersley2D(uint i, uint N)
{
    // Radical inverse based on http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
    uint bits = reversebits(i);
    float rdi = float(bits) * 2.3283064365386963e-10;
    return float2(float(i) / float(N), rdi);
}

// Based on http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_slides.pdf
float3 ImportanceSampleGGX(float2 Xi, float PerceptualRoughness, float3 N)
{
    float AlphaRoughness = PerceptualRoughness * PerceptualRoughness;
    float a2 = AlphaRoughness * AlphaRoughness;
    
    float Phi = 2.0 * PI * Xi.x;
    float CosTheta = sqrt(saturate((1.0 - Xi.y) / (1.0 + (a2 - 1.0) * Xi.y)));
    float SinTheta = sqrt(saturate(1.0 - CosTheta * CosTheta));
    float3 H;
    H.x = SinTheta * cos(Phi);
    H.y = SinTheta * sin(Phi);
    H.z = CosTheta;
    float3 UpVector = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 TangentX = normalize(cross(UpVector, N));
    float3 TangentY = cross(N, TangentX);
    // Tangent to world space
    return TangentX * H.x + TangentY * H.y + N * H.z;
}

// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games, Equation 3.
float NormalDistribution_GGX(float NdotH, float AlphaRoughness)
{
    // "Sampling the GGX Distribution of Visible Normals" (2018) by Eric Heitz - eq. (1)
    // https://jcgt.org/published/0007/04/01/

    // Make sure we reasonably handle AlphaRoughness == 0
    // (which corresponds to delta function)
    AlphaRoughness = max(AlphaRoughness, 1e-3);

    float a2 = AlphaRoughness * AlphaRoughness;
    float nh2 = NdotH * NdotH;
    float f = nh2 * a2 + (1.0 - nh2);
    return a2 / max(PI * f * f, 1e-9);
}

// Smith GGX masking function G1
// [1] "Sampling the GGX Distribution of Visible Normals" (2018) by Eric Heitz - eq. (2)
// https://jcgt.org/published/0007/04/01/
float SmithGGXMasking(float NdotV, float AlphaRoughness)
{
    float a2 = AlphaRoughness * AlphaRoughness;

    // In [1], eq. (2) is defined for the tangent-space view direction V:
    //
    //                                        1
    //      G1(V) = -----------------------------------------------------------
    //                                    {      (ax*V.x)^2 + (ay*V.y)^2)  }
    //               1 + 0.5 * ( -1 + sqrt{ 1 + -------------------------- } )
    //                                    {              V.z^2             }
    //
    // Note that [1] uses notation N for the micronormal, but in our case N is the macronormal,
    // while micronormal is H (aka the halfway vector).
    //
    // After multiplying both nominator and denominator by 2*V.z and given that in our
    // case ax = ay = a, we get:
    //
    //                                2 * V.z                                        2 * V.z
    //      G1(V) = ------------------------------------------- =  ----------------------------------------
    //               V.z + sqrt{ V.z^2 + a2 * (V.x^2 + V.y^2) }     V.z + sqrt{ V.z^2 + a2 * (1 - V.z^2) }
    //
    // Since V.z = NdotV, we finally get:

    float Denom = NdotV + sqrt(a2 + (1.0 - a2) * NdotV * NdotV);
    return 2.0 * max(NdotV, 0.0) / max(Denom, 1e-6);
}


// Returns the probability of sampling direction L for the view direction V and normal N
// using the visible normals distribution.
// [1] "Sampling the GGX Distribution of Visible Normals" (2018) by Eric Heitz
// https://jcgt.org/published/0007/04/01/
float SmithGGXSampleDirectionPDF(float3 V, float3 N, float3 L, float AlphaRoughness)
{
    // Micronormal is the halfway vector
    float3 H = normalize(V + L);

    float NdotH = dot(H, N);
    float NdotV = dot(N, V);
    float NdotL = dot(N, L);
    //float VdotH = dot(V, H);
    if (NdotH > 0.0 && NdotV > 0.0 && NdotL > 0.0)
    {
        // Note that [1] uses notation N for the micronormal, but in our case N is the macronormal,
        // while micronormal is H (aka the halfway vector).
        float NDF = NormalDistribution_GGX(NdotH, AlphaRoughness); // (1) - D(N)
        float G1 = SmithGGXMasking(NdotV, AlphaRoughness); // (2) - G1(V)

        float VNDF = G1 /* * VdotH */ * NDF / NdotV; // (3) - Dv(N)
        return VNDF / (4.0 /* * VdotH */); // (17) - VdotH cancels out
    }
    else
    {
        return 0.0;
    }
}

// Visibility = G2(v,l,a) / (4 * (n,v) * (n,l))
// see https://google.github.io/filament/Filament.md.html#materialsystem/specularbrdf
float SmithGGXVisibilityCorrelated(float NdotL, float NdotV, float AlphaRoughness)
{
    // G1 (masking) is % microfacets visible in 1 direction
    // G2 (shadow-masking) is % microfacets visible in 2 directions
    // If uncorrelated:
    //    G2(NdotL, NdotV) = G1(NdotL) * G1(NdotV)
    //    Less realistic as higher points are more likely visible to both L and V
    //
    // https://ubm-twvideo01.s3.amazonaws.com/o1/vault/gdc2017/Presentations/Hammon_Earl_PBR_Diffuse_Lighting.pdf

    float a2 = AlphaRoughness * AlphaRoughness;

    float GGXV = NdotL * sqrt(max(NdotV * NdotV * (1.0 - a2) + a2, 1e-7));
    float GGXL = NdotV * sqrt(max(NdotL * NdotL * (1.0 - a2) + a2, 1e-7));

    return 0.5 / (GGXV + GGXL);
}

float ComputeCubeMapPixelSolidAngle(float Width, float Height)
{
    return 4.0 * PI / (6.0 * Width * Height);
}

float2 IntegrateBRDF(float NoV, float PerceptualRoughness, uint NumSamples)
{
    float3 V;
    V.x = sqrt(1.0 - NoV * NoV); // sin
    V.y = 0.0;
    V.z = NoV; // cos
    const float3 N = float3(0.0, 0.0, 1.0);
    float A = 0.0;
    float B = 0.0;
    
    for (uint i = 0u; i < NumSamples; i++)
    {
        float2 Xi = Hammersley2D(i, NumSamples);
        float3 H = ImportanceSampleGGX(Xi, PerceptualRoughness, N);
        float3 L = 2.0 * dot(V, H) * H - V;
        float NoL = saturate(L.z);
        float NoH = saturate(H.z);
        float VoH = saturate(dot(V, H));
        if (NoL > 0.0)
        {
            // https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
            // Also see eq. 92 in  https://google.github.io/filament/Filament.md.html#lighting/imagebasedlights
            // Also https://www.shadertoy.com/view/3lXXDB
            // Note that VoH / (NormalDistribution_GGX(H,Roughness) * NoH) term comes from importance sampling
            float AlphaRoughness = PerceptualRoughness * PerceptualRoughness;
            float G_Vis = 4.0 * SmithGGXVisibilityCorrelated(NoL, NoV, AlphaRoughness) * VoH * NoL / NoH;
            float Fc = pow(1.0 - VoH, 5.0);
            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    
    return float2(A, B) / float(NumSamples);
}
