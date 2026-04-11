// Based on https://www.shadertoy.com/view/Dl2SDW

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

SamplerState gsamPointWrap : register(s0);

Texture2D gScene : register(t0);

float w0(float a)
{
    return (1.0 / 6.0) * (a * (a * (-a + 3.0) - 3.0) + 1.0);
}

float w1(float a)
{
    return (1.0 / 6.0) * (a * a * (3.0 * a - 6.0) + 4.0);
}

float w2(float a)
{
    return (1.0 / 6.0) * (a * (a * (-3.0 * a + 3.0) + 3.0) + 1.0);
}

float w3(float a)
{
    return (1.0 / 6.0) * (a * a * a);
}

// g0 and g1 are the two amplitude functions
float g0(float a)
{
    return w0(a) + w1(a);
}

float g1(float a)
{
    return w2(a) + w3(a);
}

// h0 and h1 are the two offset functions
float h0(float a)
{
    return -1.0 + w1(a) / (w0(a) + w1(a));
}

float h1(float a)
{
    return 1.0 + w3(a) / (w2(a) + w3(a));
}

float4 texture_bicubic(Texture2D tex, float2 uv, float4 texelSize, float2 fullSize, float lod)
{
    uv = uv * texelSize.zw + 0.5;
    float2 iuv = floor(uv);
    float2 fuv = frac(uv);

    float g0x = g0(fuv.x);
    float g1x = g1(fuv.x);
    float h0x = h0(fuv.x);
    float h1x = h1(fuv.x);
    float h0y = h0(fuv.y);
    float h1y = h1(fuv.y);

    float2 p0 = (float2(iuv.x + h0x, iuv.y + h0y) - 0.5) * texelSize.xy;
    float2 p1 = (float2(iuv.x + h1x, iuv.y + h0y) - 0.5) * texelSize.xy;
    float2 p2 = (float2(iuv.x + h0x, iuv.y + h1y) - 0.5) * texelSize.xy;
    float2 p3 = (float2(iuv.x + h1x, iuv.y + h1y) - 0.5) * texelSize.xy;
	
    float2 lodFudge = pow(1.95, lod) / fullSize;
    return g0(fuv.y) * (g0x *
        tex.SampleLevel(gsamPointWrap, p0, lod) +
                        
        g1x * tex.SampleLevel(gsamPointWrap, p1, lod)
                    ) +
           g1(fuv.y) * (
            g0x * tex.SampleLevel(gsamPointWrap, p2, lod) +
                        g1x * tex.SampleLevel(gsamPointWrap, p3, lod));
}


float4 textureBicubic(Texture2D s, float2 uv, float lod)
{
    uint w0, h0;
    uint w1, h1;
    uint w2, h2;
    uint n;
    s.GetDimensions(int(lod), w0, h0, n);
    s.GetDimensions(int(lod + 1.0), w1, h1, n);
    s.GetDimensions(0, w2, h2, n);
    
    float2 lodSizeFloor = float2(w0, h0);
    float2 lodSizeCeil = float2(w1, h1);
    float2 fullSize = float2(w2, h2);
    float4 floorSample = texture_bicubic(s, uv, float4(1.0 / lodSizeFloor.x, 1.0 / lodSizeFloor.y, lodSizeFloor.x, lodSizeFloor.y), fullSize, floor(lod));
    float4 ceilSample = texture_bicubic(s, uv, float4(1.0 / lodSizeCeil.x, 1.0 / lodSizeCeil.y, lodSizeCeil.x, lodSizeCeil.y), fullSize, ceil(lod));
    return lerp(floorSample, ceilSample, frac(lod));
}

float4 PS(VertexOut pIn) : SV_TARGET
{
    return textureBicubic(gScene, pIn.TexC, 0.5);
}