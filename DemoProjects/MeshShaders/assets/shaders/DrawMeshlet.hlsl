
struct Vertex
{
    float3 Pos;
    float3 Normal;
    float3 TangentU;
    float2 TexC;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 Normal : NORMAL;
    uint MeshletId : BLENDINDICES;
};

struct Meshlet
{
    uint VertexOffset;
    uint TriangleOffset;
    uint VertexCount;
    uint TriangleCount;
};

StructuredBuffer<Vertex> Vertices : register(t0);
StructuredBuffer<Meshlet> Meshlets : register(t1);
StructuredBuffer<uint> MeshletVertices : register(t2);
StructuredBuffer<uint> MeshletIndices : register(t3);

cbuffer cbPass : register(b0)
{
    float4x4 gWorld;
    float4x4 gViewProj;
    float3 gCameraPos;
    uint gPadding;
}

[numthreads(128, 1, 1)]
[outputtopology("triangle")]
void MS(
    uint tid : SV_GroupThreadID,
    uint gid : SV_GroupID,
    out vertices VertexOut outVerts[64],
    out indices uint3 outTris[126]
)
{
    Meshlet m = Meshlets[gid];
    
    SetMeshOutputCounts(m.VertexCount, m.TriangleCount);
    
    if (tid < m.VertexCount)
    {
        uint mIdx = MeshletVertices[m.VertexOffset + tid];
        
        VertexOut vOut;
        vOut.PosW = mul(float4(Vertices[mIdx].Pos, 1), gWorld);
        vOut.PosH = mul(float4(vOut.PosW, 1), gViewProj);
        vOut.Normal = mul(float4(Vertices[mIdx].Normal, 0), gWorld);
        vOut.MeshletId = gid;
        
        outVerts[tid] = vOut;
    }
    
    if (tid < m.TriangleCount)
    {
        uint packedIdx = MeshletIndices[m.TriangleOffset + tid];
        uint3 tris = uint3(packedIdx & 0x3FF, (packedIdx >> 10) & 0x3FF, (packedIdx >> 20) & 0x3FF);
        
        outTris[tid] = tris;
    }
}

float4 PS(VertexOut vOut) : SV_TARGET
{
    const float3 colors[] =
    {
        float3(1, 0, 0),
        float3(0, 1, 0),
        float3(0, 0, 1),
        float3(1, 1, 0),
        float3(1, 0, 1),
        float3(0, 1, 1),
    };
    
    float3 lightDir = float3(-1, -1, 1);
    
    vOut.Normal = normalize(vOut.Normal);
    float3 L = normalize(-lightDir);
    float3 V = normalize(gCameraPos - vOut.PosW);
    float3 H = normalize(vOut.Normal + V);
    
    float NdotL = saturate(dot(vOut.Normal, L));
    float NdotH = saturate(dot(vOut.Normal, H));
    NdotH = NdotL != 0 ? NdotH : 0.0f;
    NdotH = pow(NdotH, 16);
    
    float3 color = colors[vOut.MeshletId % 6] * (NdotL + NdotH + 0.2);
    
    return float4(color, 1);

}