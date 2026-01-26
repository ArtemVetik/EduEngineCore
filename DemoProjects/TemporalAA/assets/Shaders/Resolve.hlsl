Texture2D gCurrentTex : register(t0);
Texture2D gHistoryTex : register(t1);
Texture2D gVelocityTex : register(t2);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

cbuffer cbConstants : register(b0)
{
    float2 gTextureSize;
    uint gVelocityMode;
    uint gPadding;
};

float CatmullRomWeight(float x)
{
    x = abs(x);

    float x2 = x * x;
    float x3 = x2 * x;

    if (x <= 1.0)
        return 1.5 * x3 - 2.5 * x2 + 1.0;
    else if (x < 2.0)
        return -0.5 * x3 + 2.5 * x2 - 4.0 * x + 2.0;

    return 0.0;
}

float3 SampleCatmullRom(Texture2D tex, SamplerState samp, float2 uv, float2 texSize)
{
    float2 texel = uv * texSize - 0.5;
    float2 base = floor(texel);
    float2 f = texel - base;

    float3 result = 0;
    float totalWeight = 0;

    for (int y = -1; y <= 2; y++)
    {
        for (int x = -1; x <= 2; x++)
        {
            float2 offset = float2(x, y);
            float2 coord = (base + offset + 0.5) / texSize;

            float wx = CatmullRomWeight(f.x - offset.x);
            float wy = CatmullRomWeight(f.y - offset.y);

            float w = wx * wy;

            result += tex.SampleLevel(samp, coord, 0) * w;
            totalWeight += w;
        }
    }

    return result / totalWeight;
}

float2 GetVelocity(float2 uv)
{
    if (gVelocityMode == 0)
    {
        return gVelocityTex.Sample(gsamPointClamp, uv);
    }
    else
    {
        float maxMag = 0;
        float2 bestVelocity = 0;

        for (int y = -1; y <= 1; ++y)
        {
            for (int x = -1; x <= 1; ++x)
            {
                float2 offset = float2(x, y) / gTextureSize;
                float2 vel = gVelocityTex.Sample(gsamPointClamp, uv + offset);

                float mag = dot(vel, vel);

                if (mag > maxMag)
                {
                    maxMag = mag;
                    bestVelocity = vel;
                }
            }
        }

        return bestVelocity;
    }
}

float4 PS(VertexOut vOut) : SV_Target
{
    float2 uv = vOut.TexC;
    
    float2 velocityUV = GetVelocity(uv);
    float2 reprojectedUV = uv - velocityUV;
    
    float2 texelSize = 1.0 / gTextureSize;
    
    float3 currentColor = gCurrentTex.Sample(gsamLinearClamp, uv);
    float3 previousColor = SampleCatmullRom(gHistoryTex, gsamLinearClamp, reprojectedUV, gTextureSize);
    
    float3 minColor = currentColor, maxColor = currentColor;
    
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float3 color = gCurrentTex.Sample(gsamPointClamp, uv + float2(x, y) / gTextureSize);
            minColor = min(minColor, color);
            maxColor = max(maxColor, color);
        }
    }
    
    float3 previousColorClamped = clamp(previousColor, minColor, maxColor);
    float3 output = currentColor * 0.1 + previousColorClamped * 0.9;
    
    return float4(output, 1.0);
}