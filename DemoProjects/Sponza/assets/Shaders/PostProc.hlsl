Texture2D gSceneTex : register(t0);

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

float4 PS(VertexOut vOut) : SV_Target
{
    float3 color = gSceneTex.Sample(gsamPointClamp, vOut.TexC);
    //color = color / (color + 1);
    //color = pow(color, 1.0 / 2.2);
    
    return float4(color, 1);
}