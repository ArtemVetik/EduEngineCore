SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

Texture2D gSkyLUT : register(t0);

#define PI 3.14159265358979323846264338327950288419716939937510

cbuffer cbPass : register(b0)
{
    float4x4 gView;
    float4x4 gProj;
    float4 gMainLightColor;
    float3 gMainLightPosition;
    float gSunSize;
};

struct VertexIn
{
    float3 PosL : POSITION;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosL : TEXCOORD;
};

// Sample sky view LUT.
// https://sebh.github.io/publications/egsr2020.pdf (Section 5.3)
float4 SampleSkyLUT(float3 rayDir)
{
    rayDir = normalize(rayDir);
            
    float azimuthAngle = atan2(rayDir.x, rayDir.z); // [-π, π]
    float altitudeAngle = asin(rayDir.y); // [-π/2, π/2]
            
    float2 uv = float2(
                    (azimuthAngle + PI) / (2.0 * PI),
                    0.5 + 0.5 * sign(altitudeAngle) * sqrt(abs(altitudeAngle) * 2.0 / PI)
                );

    return gSkyLUT.Sample(gsamLinearWrap, uv);
}

// Calculates the sun shape (both arguments must be unit view/light directions).
// Code source: https://github.com/TwoTailsGames/Unity-Built-in-Shaders/blob/master/DefaultResourcesExtra/Skybox-Procedural.shader
float SunShape(float3 lightDir, float3 viewDir)
{
    if (viewDir.y <= 0.0)
        return 0.0;

    float3 delta = lightDir - viewDir;
    float dist = length(delta);
    float spot = 1.0 - smoothstep(0.0, gSunSize, dist);
    return spot * spot;
}

VertexOut VS(VertexIn vIn)
{
    VertexOut output;
    
    output.PosL = vIn.PosL;
    
    float4x4 viewNoTrans = gView;
    viewNoTrans._41 = 0;
    viewNoTrans._42 = 0;
    viewNoTrans._43 = 0;
    
    float4 clipPos = mul(mul(float4(vIn.PosL, 1), viewNoTrans), gProj);
    clipPos.z = 0;
    
    output.PosH = clipPos;
    
    return output;
}

float4 PS(VertexOut vOut) : SV_Target
{
    float3 viewDir = normalize(vOut.PosL);
    float3 lightDir = normalize(gMainLightPosition);

    float4 sunColor = SunShape(lightDir, viewDir) * gMainLightColor;
    float4 skyColor = SampleSkyLUT(viewDir) * 2;

    return sunColor + skyColor;
}