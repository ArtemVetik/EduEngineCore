
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

struct CullData
{
    float3 sphere_center;
    float radius;
    float3 cone_apex;
    int coneAxisCutoffPacked;
};

struct VisibleMeshlets
{
    uint meshletIndex[32];
};

StructuredBuffer<Vertex> gVertices : register(t0);
StructuredBuffer<Meshlet> gMeshlets : register(t1);
StructuredBuffer<CullData> gCullData : register(t2);
StructuredBuffer<uint> gMeshletVertices : register(t3);
StructuredBuffer<uint> gMeshletIndices : register(t4);

cbuffer cbPass : register(b0)
{
    float4x4 gWorld;
    float4x4 gViewProj;
    float3 gCameraPos;
    float gScale;
    float4 gPlanes[6];
}

cbuffer cbInstance : register(b1)
{
    uint gMeshletsCount;
    float3 gPadding2;
}

groupshared VisibleMeshlets sVisibleMeshlets;

float3 HashColor(uint id)
{
    id ^= id >> 16;
    id *= 0x7feb352d;
    id ^= id >> 15;
    id *= 0x846ca68b;
    id ^= id >> 16;

    return float3(
        (id & 255) / 255.0,
        ((id >> 8) & 255) / 255.0,
        ((id >> 16) & 255) / 255.0
    );
}

bool IsVisible(CullData c)
{
    float3 center = mul(float4(c.sphere_center, 1), gWorld);

    [unroll]
    for (int i = 0; i < 6; i++)
    {
        if (dot(gPlanes[i], float4(center, 1)) < -c.radius * gScale)
            return false;
    }
    
    int b0 = (c.coneAxisCutoffPacked >> 0) & 0xFF;
    int b1 = (c.coneAxisCutoffPacked >> 8) & 0xFF;
    int b2 = (c.coneAxisCutoffPacked >> 16) & 0xFF;
    int b3 = (c.coneAxisCutoffPacked >> 24) & 0xFF;
    
    b0 = (b0 << 24) >> 24;
    b1 = (b1 << 24) >> 24;
    b2 = (b2 << 24) >> 24;
    b3 = (b3 << 24) >> 24;
    
    float3 coneAxis = float3(
        b0 / 127.0,
        b1 / 127.0,
        b2 / 127.0
    );
    
    coneAxis = normalize(mul(float4(coneAxis, 0), gWorld));
    float3 coneApex = mul(float4(c.cone_apex, 1), gWorld).xyz;
    
    float coneCutoff = b3 / 127.0;
    
    if (dot(coneAxis, normalize(coneApex - gCameraPos)) >= coneCutoff)
        return false;
    
    return true;
}

[numthreads(32, 1, 1)]
void AS(
    uint dtid : SV_DispatchThreadID,
    uint tid : SV_GroupThreadID,
    uint gid : SV_GroupID
)
{
    bool visible = false;
 
    if (dtid < gMeshletsCount)
        visible = IsVisible(gCullData[dtid]);

    if (visible)
    {
        uint sIdx = WavePrefixCountBits(visible);
        sVisibleMeshlets.meshletIndex[sIdx] = dtid;
    }
    
    uint groupCount = WaveActiveSum(visible);
    DispatchMesh(groupCount, 1, 1, sVisibleMeshlets);
}

[numthreads(128, 1, 1)]
[outputtopology("triangle")]
void MS(
    uint tid : SV_GroupThreadID,
    uint gid : SV_GroupID,
    in payload VisibleMeshlets visibleMeshlets,
    out vertices VertexOut outVerts[64],
    out indices uint3 outTris[126]
)
{
    uint meshletId = visibleMeshlets.meshletIndex[gid];
    Meshlet m = gMeshlets[meshletId];
    
    SetMeshOutputCounts(m.VertexCount, m.TriangleCount);
    
    if (tid < m.VertexCount)
    {
        uint mIdx = gMeshletVertices[m.VertexOffset + tid];
        
        VertexOut vOut;
        vOut.PosW = mul(float4(gVertices[mIdx].Pos, 1), gWorld);
        vOut.PosH = mul(float4(vOut.PosW, 1), gViewProj);
        vOut.Normal = mul(float4(gVertices[mIdx].Normal, 0), gWorld);
        vOut.MeshletId = meshletId;
        
        outVerts[tid] = vOut;
    }
    
    if (tid < m.TriangleCount)
    {
        uint packedIdx = gMeshletIndices[m.TriangleOffset + tid];
        uint3 tris = uint3(packedIdx & 0x3FF, (packedIdx >> 10) & 0x3FF, (packedIdx >> 20) & 0x3FF);
        
        outTris[tid] = tris;
    }
}

float4 PS(VertexOut vOut) : SV_TARGET
{
    float3 lightDir = float3(-1, -1, 1);
    
    vOut.Normal = normalize(vOut.Normal);
    float3 L = normalize(-lightDir);
    float3 V = normalize(gCameraPos - vOut.PosW);
    float3 H = normalize(vOut.Normal + V);
    
    float NdotL = saturate(dot(vOut.Normal, L));
    float NdotH = saturate(dot(vOut.Normal, H));
    NdotH = NdotL != 0 ? NdotH : 0.0f;
    NdotH = pow(NdotH, 16);
    
    float3 diffuse = HashColor(vOut.MeshletId);
    float3 color = diffuse * (NdotL + NdotH + 0.2);
    
    return float4(color, 1);

}