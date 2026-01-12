#include "Common.hlsli"

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 Normal : NORMAL;
    uint MeshletId : BLENDINDICES0;
    uint LodLevel : BLENDINDICES1;
};

struct MSPayload
{
    uint InstanceCount[MAX_LOD_LEVEL];
    uint InstanceOffset[MAX_LOD_LEVEL + 1];
    uint GroupOffset[MAX_LOD_LEVEL + 1];
    uint InstanceList[WAVE_THREADS_NUM];
};

groupshared uint s_InstanceCount[MAX_LOD_LEVEL];
groupshared uint s_InstanceOffset[MAX_LOD_LEVEL + 1];
groupshared uint s_GroupOffset[MAX_LOD_LEVEL + 1];
groupshared uint s_InstanceList[WAVE_THREADS_NUM];

[numthreads(WAVE_THREADS_NUM, 1, 1)]
void AS(
    uint dtid : SV_DispatchThreadID,
    uint tid : SV_GroupThreadID,
    uint gid : SV_GroupID
)
{
    if (tid == 0)
    {
        s_InstanceOffset[0] = 0;
        s_GroupOffset[0] = 0;
    }
    
    uint lodLevel = MAX_LOD_LEVEL;
    
    uint instanceId = dtid;
    if (instanceId < gInstanceCount)
    {
        Instance instance = gInstances[gDispatchInstanceOffset + instanceId];
        
        if (IsVisible(instance))
        {
            lodLevel = ComputeLod(instance.BoundingSphere);
        }
    }
    
    uint offset = 0;
    [unroll]
    for (int i = 0; i < MAX_LOD_LEVEL; i++)
    {
        bool sameLod = lodLevel == i;
        
        if (sameLod)
        {
            s_InstanceList[offset + WavePrefixCountBits(sameLod)] = gDispatchInstanceOffset + instanceId;
        }
        
        s_InstanceCount[i] = WaveActiveCountBits(sameLod);
        offset += s_InstanceCount[i];
    }
    
    if (tid < MAX_LOD_LEVEL)
    {
        uint instanceCount = s_InstanceCount[tid];
        s_InstanceOffset[tid + 1] = instanceCount + WavePrefixSum(instanceCount);
    }
    
    if (tid < MAX_LOD_LEVEL)
    {
        uint packCount = min(MAX_VERTICES / gMeshletInfoPacked[tid].y, MAX_TRIS / gMeshletInfoPacked[tid].z);
        uint groupOffset = s_InstanceCount[tid] * (gMeshletInfoPacked[tid].x - 1) + DivRoundUp(s_InstanceCount[tid], packCount);
        s_GroupOffset[tid + 1] = groupOffset + WavePrefixSum(groupOffset);
    }
    
    MSPayload msPayload;
    msPayload.InstanceList[tid] = s_InstanceList[tid];
    
    if (tid < MAX_LOD_LEVEL)
    {
        msPayload.InstanceCount[tid] = s_InstanceCount[tid];
        msPayload.InstanceOffset[tid] = s_InstanceOffset[tid];
        msPayload.GroupOffset[tid] = s_GroupOffset[tid];
    }

    DispatchMesh(s_GroupOffset[MAX_LOD_LEVEL], 1, 1, msPayload);
}

[numthreads(128, 1, 1)]
[outputtopology("triangle")]
void MS(
    uint tid : SV_GroupThreadID,
    uint gid : SV_GroupID,
    in payload MSPayload msPayload,
    out vertices VertexOut outVerts[MAX_VERTICES],
    out indices uint3 outTris[MAX_TRIS]
)
{
    uint threadLod = 0;
    uint laneIndex = tid % WaveGetLaneCount();
    
    if (laneIndex < MAX_LOD_LEVEL)
    {
        threadLod = WaveActiveCountBits(gid >= msPayload.GroupOffset[laneIndex]) - 1;
    }
    
    uint lodLevel = WaveReadLaneFirst(threadLod);
    
    uint meshletInstanceId = gid - msPayload.GroupOffset[lodLevel];
    
    uint instanceOffset = meshletInstanceId % msPayload.InstanceCount[lodLevel];
    uint meshletId = meshletInstanceId / msPayload.InstanceCount[lodLevel];
    
    uint instanceCount = 1;
    if (meshletId == gMeshletInfoPacked[lodLevel].x - 1)
    {
        uint packCount = min(MAX_VERTICES / gMeshletInfoPacked[lodLevel].y, MAX_TRIS / gMeshletInfoPacked[lodLevel].z);
        instanceOffset *= packCount;

        instanceCount = min(msPayload.InstanceCount[lodLevel] - instanceOffset, packCount);
    }
    else
    {
        uint instanceId = msPayload.InstanceList[msPayload.InstanceOffset[lodLevel] + instanceOffset];
        Instance instance = gInstances[instanceId];
        
        if (!IsVisible(gCullData[lodLevel][meshletId], instance.World))
            instanceCount = 0;
    }
    
    Meshlet m = gMeshlets[lodLevel][meshletId];
    SetMeshOutputCounts(m.VertexCount * instanceCount, m.TriangleCount * instanceCount);
    
    if (tid < m.VertexCount * instanceCount)
    {
        uint localInstance = tid / m.VertexCount;
        uint instanceId = msPayload.InstanceList[msPayload.InstanceOffset[lodLevel] + instanceOffset + localInstance];
        Instance instance = gInstances[instanceId];
        
        uint mIdx = gMeshletVertices[lodLevel][m.VertexOffset + tid % m.VertexCount];
        
        VertexOut vOut;
        vOut.PosW = mul(float4(gVertices[lodLevel][mIdx].Pos, 1), instance.World);
        vOut.PosH = mul(float4(vOut.PosW, 1), gViewProj);
        vOut.Normal = mul(float4(gVertices[lodLevel][mIdx].Normal, 0), instance.World);
        vOut.MeshletId = meshletId;
        vOut.LodLevel = lodLevel;
        
        outVerts[tid] = vOut;
    }
    
    if (tid < m.TriangleCount * instanceCount)
    {
        uint localInstance = tid / m.TriangleCount;

        uint packedIdx = gMeshletIndices[lodLevel][m.TriangleOffset + tid % m.TriangleCount];
        uint3 tris = uint3(packedIdx & 0x3FF, (packedIdx >> 10) & 0x3FF, (packedIdx >> 20) & 0x3FF);
        
        outTris[tid] = tris + localInstance * m.VertexCount;
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
    
    float3 diffuse = 1;
    
    if (gRenderMode == 0)
        diffuse = HashColor(vOut.MeshletId);
    else if (gRenderMode == 1)
        diffuse = LODColorHSV(vOut.LodLevel, gLodCount);
    
    float3 color = diffuse * (NdotL + NdotH + 0.2);
    
    return float4(color, 1);

}