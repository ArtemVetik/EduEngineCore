struct Particle
{
    float3 Position;
    float3 Color;
    float3 Velocity;
    float Lifetime;
    float Age;
};

struct VS_Out
{
    float4 PosH : SV_POSITION;
    float3 Color : COLOR;
};

struct GS_Out
{
    float4 PosH : SV_POSITION;
    float3 Color : COLOR;
    float2 TexC : TEXCOORD;
};