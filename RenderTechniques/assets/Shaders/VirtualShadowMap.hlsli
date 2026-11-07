#ifndef VIRTUAL_SHADOW_MAP_HLSLI
#define VIRTUAL_SHADOW_MAP_HLSLI

#include "Shared/VirtualShadowMapDefinitions.h"

#define PM_ALLOCATED 0x1

#define PT_MAPPED     (1u << 31)   // page has slot (from cache or new)
#define PT_REQUESTED  (1u << 30)   // page is visible in this frame (Mark)
#define PT_OVERFLOW   (1u << 29)   // page is needed, but there were no available slots
#define PT_RENDER     (1u << 28)   // page is needed for rendering
#define PT_SLOT_MASK  0xFFFFu

#define LIST_LRU        0 // last frame's order, requested/empty entries replaced by -1
#define LIST_AVAILABLE  1 // packed LRU + empty at the end; allocation pops from the end. [ LRU...LRU, EMPTY...EMPTY ]
#define LIST_EMPTY      2 // invalid pages, appended to AVAILABLE after packing
#define LIST_REQUESTED  3 // pages used this frame; becomes next frame's LRU order

uint2 VSM_PageTableAddress(uint2 page, uint level)
{
    return page + uint2(0, level * VSM_PAGES_XY);
}

uint VSM_SelectClipmap(float3 worldPos, float3 cameraPos, float lodBias)
{
    float3 delta = worldPos - cameraPos;

    // log2(d^2) / 2 = log2(d)
    int level = int(floor(0.5 * log2(max(dot(delta, delta), 1e-12)) + lodBias));
    // Keep distant receivers on the coarsest level; its UV bounds define coverage.
    return uint(clamp(level - VSM_FIRST_CLIPMAP_LEVEL, 0, VSM_NUM_CLIPMAPS - 1));
}

struct PhysMetaEntry
{
    uint Flags;
    uint LastRequestedFrame;
    uint AllocFrame;
    uint2 PageAddr;
    uint LevelIndex;
};

// Wave-aggregated InterlockedAdd: one atomic per wave, returns this lane's original value
int WaveInterlockedAddScalar(RWStructuredBuffer<int> buf, uint index, int value)
{
    int waveTotal = int(WaveActiveCountBits(true)) * value;

    int origin = 0;
    if (WaveIsFirstLane())
        InterlockedAdd(buf[index], waveTotal, origin);

    origin = WaveReadLaneFirst(origin) + int(WavePrefixCountBits(true)) * value;
    return origin;
}

// ---------------------------
// ---------- Lists ----------
// ---------------------------

// Layout: VSM_MAX_PHYS_PAGES items followed by a counter
// [ i0, i1, i2, i3, ..., Counter ]
uint ListStart(uint list)
{
    return list * (VSM_MAX_PHYS_PAGES + 1);
}

uint ListCounter(uint list)
{
    return ListStart(list) + VSM_MAX_PHYS_PAGES;
}

bool PushList(RWStructuredBuffer<int> lists, uint list, int slot)
{
    int offset = WaveInterlockedAddScalar(lists, ListCounter(list), 1);

    // Overflow here means a physical page is lost forever - must never happen
    // If that happened, it means something went wrong before that
    if (offset >= VSM_MAX_PHYS_PAGES)
        return false;

    lists[ListStart(list) + offset] = slot;
    return true;
}

int PopList(RWStructuredBuffer<int> lists, uint list)
{
    int offset = WaveInterlockedAddScalar(lists, ListCounter(list), -1);

    offset -= 1; // index after decrement; counter may go negative on overflow
    return offset < 0 ? -1 : lists[ListStart(list) + offset];
}

// ---------------------------------------
// ---------- Building VSM Data ----------
// ---------------------------------------

struct VSMParams
{
    float4x4 WorldToShadowUV; // [0,1]^2
    float4x4 WorldToShadowUVNormal; // inverse-transpose of WorldToShadowUV
    uint PageTable;
    uint PhysPool;
    uint LevelIndex;
    float ShadowDepthScale;
};

struct VSMAddress
{
    bool InWindow;
    float2 VirtualTexelFloat;
    uint2 VirtualTexel; // 0..(VSM_SIZE-1)
    uint2 Page; // 0..(VSM_SIZE/PAGE_SIZE-1)
    float Depth;
};

VSMAddress VSM_WorldToVirtual(VSMParams p, float3 worldPos)
{
    VSMAddress a;
    float3 uvz = mul(float4(worldPos, 1), p.WorldToShadowUV).xyz;
    a.InWindow = all(uvz.xy >= 0) && all(uvz.xy < 1);
    a.VirtualTexelFloat = uvz.xy * VSM_VIRTUAL_SIZE;
    a.VirtualTexel = uint2(a.VirtualTexelFloat);
    a.Page = a.VirtualTexel >> VSM_LOG2_PAGE_SIZE;
    a.Depth = uvz.z;
    return a;
}

uint2 VSM_SlotToPhysPage(uint slot)
{
    return uint2(slot & (VSM_POOL_SIZE - 1), slot / VSM_POOL_SIZE);
}

uint2 VSM_SlotToPhysTexel(VSMParams p, uint slot, uint2 virtualTexel)
{
    uint2 slotXY = VSM_SlotToPhysPage(slot);
    return slotXY * VSM_PAGE_SIZE + (virtualTexel & (VSM_PAGE_SIZE - 1));
}

// ------------------------------
// ---------- Sampling ----------
// ------------------------------

struct VSMSample
{
    VSMAddress Address;
    bool Valid;
    uint Entry;
    float OccluderDepth;
    float ReceiverDepth;
};

VSMSample VSM_Sample(VSMParams p, float3 worldPos)
{
    VSMSample s = (VSMSample) 0;
    VSMAddress a = VSM_WorldToVirtual(p, worldPos);
    s.Address = a;
    s.ReceiverDepth = a.Depth;

    if (!a.InWindow)
        return s;

    RWTexture2D<uint> pageTable = ResourceDescriptorHeap[p.PageTable]; // TODO: pageTable now is UAV. Create SRV
    s.Entry = pageTable[VSM_PageTableAddress(a.Page, p.LevelIndex)];

    if (!(s.Entry & PT_MAPPED))
        return s;

    RWTexture2D<uint> pool = ResourceDescriptorHeap[p.PhysPool];
    s.OccluderDepth = asfloat(pool[VSM_SlotToPhysTexel(p, s.Entry & PT_SLOT_MASK, a.VirtualTexel)]);
    s.Valid = true;

    return s;
}

float VSM_ComputeOptimalSlopeBias(VSMParams p, VSMSample s, float3 worldPos, float3 normal, float maxSlopeOffsetWorld)
{
    // Ax + By + Cz + D = 0
    // D = -dot(N, P)
    float4 plane = float4(normal, -dot(normal, worldPos));
    plane = mul(plane, p.WorldToShadowUVNormal);

    // z = -(Ax + By + D) / C
    float planeZ = abs(plane.z) < 1e-8f
        ? (plane.z < 0.0f ? -1e-8f : 1e-8f)
        : plane.z;
    float2 depthSlope = -plane.xy / planeZ;

    float2 center = float2(s.Address.VirtualTexel) + 0.5f;
    float2 offsetUV = (center - s.Address.VirtualTexelFloat) / float(VSM_VIRTUAL_SIZE);

    float slopeBias = 2.0 * max(0.0, dot(depthSlope, offsetUV));
    return min(slopeBias, maxSlopeOffsetWorld * p.ShadowDepthScale);
}

float VSM_ShadowFactor(VSMParams p, float3 worldPos, float3 normal, float rayStartDistance, float maxSlopeOffsetWorld)
{
    VSMSample s = VSM_Sample(p, worldPos);
    if (!s.Valid)
        return 1.0;

    float slopeBias = VSM_ComputeOptimalSlopeBias(p, s, worldPos, normal, maxSlopeOffsetWorld);
    float rayStartBias = rayStartDistance * p.ShadowDepthScale;
    float biasedOccluderDepth = s.OccluderDepth - slopeBias - rayStartBias;
    return s.ReceiverDepth >= biasedOccluderDepth ? 1.0 : 0.0; // reverse-Z
}

#endif // VIRTUAL_SHADOW_MAP_HLSLI
