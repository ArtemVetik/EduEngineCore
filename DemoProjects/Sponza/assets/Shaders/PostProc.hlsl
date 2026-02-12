SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

cbuffer cbPass : register(b0)
{
    uint gSceneTextureIdx;
    uint gSSRTextureIdx;
}

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

float4 PS(VertexOut vOut) : SV_Target
{
    Texture2D sceneTex = ResourceDescriptorHeap[gSceneTextureIdx];
    float3 color = sceneTex.Sample(gsamPointClamp, vOut.TexC).xyz;
    
#if defined(DEBUG_VIEW) && DEBUG_VIEW > 0
    return float4(color, 1);
#endif
    
    Texture2D ssrTex = ResourceDescriptorHeap[gSSRTextureIdx];
    float3 ssrColor = ssrTex.Sample(gsamPointClamp, vOut.TexC).xyz;
    color += ssrColor;
    
    color = color / (color + 1);
    color = pow(color, 1.0 / 2.2);
    
    return float4(color, 1);
}