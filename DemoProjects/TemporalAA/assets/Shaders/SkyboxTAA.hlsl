SamplerState gSamLinearClamp : register(s3);

TextureCube gCubeMap : register(t0);

cbuffer cbPass : register(b0)
{
    float4x4 gView;
    float4x4 gProj;
    float gLod;
    uint3 gPadding;
};

struct VertexIn
{
    float3 PosL : POSITION;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosL : POSITION;
};

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
    float3 color = gCubeMap.SampleLevel(gSamLinearClamp, vOut.PosL, gLod).rgb;
    
    
    
    return float4(color, 1);
}