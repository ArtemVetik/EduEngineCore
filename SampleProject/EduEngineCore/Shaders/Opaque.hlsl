SamplerState gAlbedo_sampler : register(s3);
SamplerState gTestTex_sampler : register(s4);

Buffer<float4> gArray[13] : register(t4);
Texture2D gAlbedo : register(t0);
Texture2D gTestTex : register(t3);
StructuredBuffer<float> gStructBuff : register(t1);

cbuffer cbPerObject : register(b0)
{
	float4x4 gWorld; 
};

cbuffer cbPerPass : register(b1)
{
    float4x4 gViewProj;
};

struct VertexIn
{
    float3 PosL     : POSITION;
    float3 NormalL  : NORMAL;
    float3 TangentU : TANGENT;
    float2 TexC     : TEXCOORD;
};

struct VertexOut
{
	float4 PosH  : SV_POSITION;
    float2 TexC : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;
	
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    
    float4 arrayFirst = gArray[0].Load(0);
    
    vout.PosH = mul(posW, gViewProj);
    vout.TexC = vin.TexC;
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    float a = gStructBuff.Load(0);
    float4 b = gTestTex.Sample(gTestTex_sampler, a);
    
    return gAlbedo.Sample(gAlbedo_sampler, pin.TexC + b.xy);
}


