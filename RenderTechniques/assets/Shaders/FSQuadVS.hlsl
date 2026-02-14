
static const float2 pos[3] =
{
    float2(-1, -1),
    float2(-1, +3),
    float2(+3, -1)
};

static const float2 uv[3] =
{
    float2(0, 1),
    float2(0, -1),
    float2(2, 1)
};


struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

VertexOut VS(uint vertexId : SV_VertexID)
{
    VertexOut output;
    output.PosH = float4(pos[vertexId], 0, 1);
    output.TexC = uv[vertexId];
    
    return output;
}