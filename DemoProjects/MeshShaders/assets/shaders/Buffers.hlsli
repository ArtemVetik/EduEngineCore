#include "Shared.h"

struct Vertex
{
    float3 Pos;
    float3 Normal;
    float3 TangentU;
    float2 TexC;
};

struct CullData
{
    float3 SphereCenter;
    float SphereRadius;
    float3 ConeApex;
    uint ConeAxisCutoffPacked;
};

StructuredBuffer<Vertex> gVertices[MAX_LOD_LEVEL] : register(t0, space0);
StructuredBuffer<Meshlet> gMeshlets[MAX_LOD_LEVEL] : register(t0, space1);
StructuredBuffer<CullData> gCullData[MAX_LOD_LEVEL] : register(t0, space2);
StructuredBuffer<uint> gMeshletVertices[MAX_LOD_LEVEL] : register(t0, space3);
StructuredBuffer<uint> gMeshletIndices[MAX_LOD_LEVEL] : register(t0, space4);
StructuredBuffer<Instance> gInstances : register(t0, space5);

cbuffer cbPass : register(b0)
{
    float4x4 gViewProj;
    
    float3 gCameraPos;
    uint gInstanceCount;
    
    float gInvTanHalfFovY; // 1.0 / tan(fovY * 0.5)
    uint gLodCount;
    uint gRenderMode;
    float gScale;
    
    uint gFlags;
    uint3 gPadding0;
    
    float4 gPlanes[6];
}

cbuffer cbDispatchData : register(b1)
{
    uint gDispatchInstanceOffset;
    uint gDispatchInstanceCount;
    uint2 gPadding1;
}

cbuffer cbMeshletData : register(b2)
{
    // x - meshletCount
    // y - lastMeshletVertices
    // z - lastMeshletTris
    uint4 gMeshletInfoPacked[MAX_LOD_LEVEL];
}