SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);

cbuffer cbPass : register(b0)
{
    uint gBloomTex0Idx;
    uint gBloomTex1Idx;
}

cbuffer cbConstants : register(b1)
{
    float3 gTint;
    float gThreshold;
    float gIntensity;
    float gScatter;
}

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD0;
};

float4 PSThreshold(VertexOut pIn) : SV_Target
{
    Texture2D bloomTex0 = ResourceDescriptorHeap[gBloomTex0Idx];
    float3 lightColor = bloomTex0.Sample(gsamLinearClamp, pIn.TexC).rgb;

    float luminance = dot(lightColor, float3(0.2126, 0.7152, 0.0722));
    float3 thresholdColor = step(gThreshold, luminance) * lightColor;
    return float4(thresholdColor, 1.0f);
}

float4 PSBlurH(VertexOut pIn) : SV_Target
{
    Texture2D bloomTex1 = ResourceDescriptorHeap[gBloomTex1Idx];
    
    float width, height;
    bloomTex1.GetDimensions(width, height);
    
    float2 texelSize = float2(1.0f / width, 0.0f);
    
    float3 c0 = bloomTex1.Sample(gsamLinearClamp, pIn.TexC - float2(texelSize.x * 4.0, 0.0)).rgb;
    float3 c1 = bloomTex1.Sample(gsamLinearClamp, pIn.TexC - float2(texelSize.x * 3.0, 0.0)).rgb;
    float3 c2 = bloomTex1.Sample(gsamLinearClamp, pIn.TexC - float2(texelSize.x * 2.0, 0.0)).rgb;
    float3 c3 = bloomTex1.Sample(gsamLinearClamp, pIn.TexC - float2(texelSize.x * 1.0, 0.0)).rgb;
    float3 c4 = bloomTex1.Sample(gsamLinearClamp, pIn.TexC).rgb;
    float3 c5 = bloomTex1.Sample(gsamLinearClamp, pIn.TexC + float2(texelSize.x * 1.0, 0.0)).rgb;
    float3 c6 = bloomTex1.Sample(gsamLinearClamp, pIn.TexC + float2(texelSize.x * 2.0, 0.0)).rgb;
    float3 c7 = bloomTex1.Sample(gsamLinearClamp, pIn.TexC + float2(texelSize.x * 3.0, 0.0)).rgb;
    float3 c8 = bloomTex1.Sample(gsamLinearClamp, pIn.TexC + float2(texelSize.x * 4.0, 0.0)).rgb;
    
    float3 color = c0 * 0.01621622 + c1 * 0.05405405 + c2 * 0.12162162 + c3 * 0.19459459
                 + c4 * 0.22702703
                 + c5 * 0.19459459 + c6 * 0.12162162 + c7 * 0.05405405 + c8 * 0.01621622;
    
    return float4(color, 1.0f);
}

float4 PSBlurV(VertexOut pIn) : SV_Target
{
    Texture2D bloomTex1 = ResourceDescriptorHeap[gBloomTex1Idx];
    
    float width, height;
    bloomTex1.GetDimensions(width, height);
    
    float2 texelSize = float2(0.0f, 1.0f / height);

    // Optimized bilinear 5-tap gaussian on the same-sized source (9-tap equivalent)
    float3 c0 = bloomTex1.Sample(gsamLinearClamp, pIn.TexC - float2(0, texelSize.y * 3.23076923)).rgb;
    float3 c1 = bloomTex1.Sample(gsamLinearClamp, pIn.TexC - float2(0, texelSize.y * 1.38461538)).rgb;
    float3 c2 = bloomTex1.Sample(gsamLinearClamp, pIn.TexC).rgb;
    float3 c3 = bloomTex1.Sample(gsamLinearClamp, pIn.TexC + float2(0, texelSize.y * 1.38461538)).rgb;
    float3 c4 = bloomTex1.Sample(gsamLinearClamp, pIn.TexC + float2(0, texelSize.y * 3.23076923)).rgb;
    
    float3 color = c0 * 0.07027027 + c1 * 0.31621622
                 + c2 * 0.22702703
                 + c3 * 0.31621622 + c4 * 0.07027027;
    
    return float4(color, 1.0f);
}

float4 PSUpscale(VertexOut pIn) : SV_Target
{
    Texture2D bloomTex0 = ResourceDescriptorHeap[gBloomTex0Idx];
    Texture2D bloomTex1 = ResourceDescriptorHeap[gBloomTex1Idx];
    
    float4 highMip = bloomTex0.Sample(gsamLinearClamp, pIn.TexC);
    float4 lowMip = bloomTex1.Sample(gsamLinearClamp, pIn.TexC);
    
    return lerp(highMip, lowMip, gScatter) * gIntensity * float4(gTint, 1.0f);
}