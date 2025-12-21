#include "ParticlesData.hlsl"

StructuredBuffer<Particle> gParticles : register(t0);

cbuffer cbPass : register(b0)
{
    float4x4 gViewProj;
    float gAspectRatio;
    uint3 gPadding;
}

VS_Out VS(uint vertexId : SV_VertexID)
{
    VS_Out output;
    
    Particle p = gParticles[vertexId];
    output.PosH = mul(float4(p.Position, 1), gViewProj);
    output.Color = p.Color;
    
    return output;
}

[maxvertexcount(4)]
void GS(point VS_Out vIn[1], inout TriangleStream<GS_Out> output)
{
    GS_Out v;
    v.Color = vIn[0].Color;
    
    v.PosH = vIn[0].PosH + float4(-1.0 / gAspectRatio, -1.0, 0, 0);
    v.TexC = float2(0, 0);
    output.Append(v);
    
    v.PosH = vIn[0].PosH + float4(-1.0 / gAspectRatio, 1.0, 0, 0);
    v.TexC = float2(0, 1);
    output.Append(v);
    
    v.PosH = vIn[0].PosH + float4(1.0 / gAspectRatio, -1.0, 0, 0);
    v.TexC = float2(1, 0);
    output.Append(v);
    
    v.PosH = vIn[0].PosH + float4(1.0 / gAspectRatio, 1.0, 0, 0);
    v.TexC = float2(1, 1);
    output.Append(v);
}

float4 PS(GS_Out vOut) : SV_Target
{
    float alpha = 1.0 - saturate(distance(vOut.TexC * 2, 1.0f));
    return float4(vOut.Color, alpha);
}