#pragma once

#define WAVE_THREADS_NUM 32

#define MAX_LOD_LEVEL 8
#define MAX_VERTICES 64
#define MAX_TRIS 126

#define DivRoundUp(x, y) ((x + y - 1) / y)

#ifdef __cplusplus
#include <EngineTypes.h>
#include "DirectXMath.h"
using float4x4 = DirectX::XMFLOAT4X4;
using float4 = DirectX::XMFLOAT4;
using float3x4 = DirectX::XMFLOAT3X4;
using uint = uint32;
#endif

struct Instance
{
    float4x4 World;
    float4 BoundingSphere;
    float3x4 gInstancePadding;
};

struct Meshlet
{
    uint VertexOffset;
    uint TriangleOffset;
    uint VertexCount;
    uint TriangleCount;
};