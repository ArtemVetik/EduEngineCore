#include "CommonTransforms.hlsli"
#include "VirtualShadowMap.hlsli"
#include "VirtualShadowMapDebug.hlsli"

#define PACK_GROUP_SIZE 1024

struct RenderListEntry
{
    uint Slot;
};

cbuffer cbObject : register(b0)
{
    float4x4 gWorld;
}

cbuffer cbPass : register(b1)
{
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float3 gCameraWorldPos;
    float gResolutionLodBias;
    uint2 gScreenSize;
    uint gFrame;
    uint gInvalidateAll;
    float3 gLightDir;
    uint gCurList;  // UAV heap undex
    uint gPrevList; // UAV heap undex
    uint gHasHistory;
    uint gDebugMode;
    float gNormalBias;
    float gMinNormalOffsetWorld;
    float gMaxSlopeOffsetWorld;
    float gScreenRayLength;
    float gPassPadding;
}

cbuffer cbBuffers : register(b2)
{
    uint gClipmapBuffer;
    uint gPhysMeta;
    uint gPageRequest;
    uint gPageTable;
    uint gRenderList;
    uint gRenderCount;
    uint gUncachedBounds;
    uint gSSDepthTexture;
    uint gNormalTexture;
    uint gClearDispatchArgs;
    uint gPhysPool;
    uint gStatsBuffer;
}

float3 VSM_LoadWorldNormal(uint2 pixel)
{
    Texture2D<float4> normalTex = ResourceDescriptorHeap[gNormalTexture];
    float3 normal = normalTex.Load(int3(pixel, 0)).xyz;
    return normal * rsqrt(max(dot(normal, normal), 1e-12f));
}

float3 VSM_OffsetReceiver(float3 worldPos, float3 normal)
{
    float distanceToCamera = length(worldPos - gCameraWorldPos);
    float offset = max(gMinNormalOffsetWorld, (gNormalBias / 1000.0f) * distanceToCamera / gProj[0][0]);
    return worldPos + normal * offset;
}

VSMParams VSM_GetParams(float3 lookupPos)
{
    RWStructuredBuffer<ClipmapLevelGPU> levels = ResourceDescriptorHeap[gClipmapBuffer];
    
    VSMParams p;
    p.LevelIndex = VSM_SelectClipmap(lookupPos, gCameraWorldPos, gResolutionLodBias);
    p.WorldToShadowUV = levels[p.LevelIndex].WorldToShadowUV;
    p.WorldToShadowUVNormal = levels[p.LevelIndex].WorldToShadowUVNormal;
    p.ShadowDepthScale = levels[p.LevelIndex].ShadowDepthScale;
    p.PageTable = gPageTable;
    p.PhysPool = gPhysPool;
    return p;
}

float VSM_ScreenRayDither(uint2 pixel)
{
    // Stable spatial dither: the demo has no temporal accumulation to filter animated noise.
    uint hash = pixel.x * 1664525u + pixel.y * 1013904223u;
    hash ^= hash >> 16;
    hash *= 2246822519u;
    hash ^= hash >> 13;
    return float(hash & 0xFFFFu) / 65536.0f;
}

bool VSM_InScreen(float3 uvz)
{
    return all(uvz.xy >= 0.0f) && all(uvz.xy < 1.0f) && uvz.z >= 0.0f && uvz.z <= 1.0f;
}

float VSM_ScreenRayCast(float3 rayOrigin, float3 rayDirection, float rayLength, float dither)
{
    if (rayLength <= 0.0f)
        return 0.0f;

    float4 startClip = mul(float4(rayOrigin, 1.0f), gViewProj);
    float4 endClip = startClip + mul(float4(rayDirection * rayLength, 0.0f), gViewProj);
    if (startClip.w <= 1e-6f || endClip.w <= 1e-6f)
        return 0.0f;

    float3 startNDC = startClip.xyz / startClip.w;
    float3 endNDC = endClip.xyz / endClip.w;
    float3 startUVz = float3(startNDC.xy * float2(0.5f, -0.5f) + 0.5f, startNDC.z);
    float3 stepUVz = float3((endNDC.xy - startNDC.xy) * float2(0.5f, -0.5f), endNDC.z - startNDC.z);
    if (!VSM_InScreen(startUVz))
        return 0.0f;

    Texture2D<float> depthTex = ResourceDescriptorHeap[gSSDepthTexture];
    float startDepth = depthTex.Load(int3(uint2(startUVz.xy * gScreenSize), 0));
    const float step = 1.0f / 4.0f;
    float sampleTime = (dither + 0.5f) * step;
    float lastSafeTime = 0.0f;

    [unroll]
    for (uint i = 0; i < 4; ++i)
    {
        float3 sampleUVz = startUVz + stepUVz * sampleTime;
        // Do not extend the bias through unobserved geometry outside the view.
        if (!VSM_InScreen(sampleUVz))
            return rayLength * lastSafeTime;

        float sampleDepth = depthTex.Load(int3(uint2(sampleUVz.xy * gScreenSize), 0));
        if (sampleDepth != startDepth && sampleUVz.z < sampleDepth) // reverse-Z
            return rayLength * max(0.0f, sampleTime - 1.5f * step);

        lastSafeTime = min(sampleTime, 1.0f);
        sampleTime += step;
    }
    return rayLength;
}

float VSM_GetRayStartDistance(float3 worldPos, float3 lookupPos, uint2 pixel)
{
    float viewDepth = max(0.0f, dot(worldPos - gCameraWorldPos, gInvView[2].xyz));
    float rayLength = gScreenRayLength * viewDepth / gProj[1][1];
    return VSM_ScreenRayCast(lookupPos, normalize(-gLightDir), rayLength, VSM_ScreenRayDither(pixel));
}

// ------------------------------
// ---------- Stats -------------
// ------------------------------

void StatsInc(uint index, bool condition = true)
{
#if VSM_GENERATE_STATS
    uint count = WaveActiveCountBits(condition);

    if (count > 0 && WaveIsFirstLane())
    {
        RWStructuredBuffer<uint> stats = ResourceDescriptorHeap[gStatsBuffer];
        InterlockedAdd(stats[index], count);
    }
#endif
}

void StatsWrite(uint index, uint value)
{
#if VSM_GENERATE_STATS
    RWStructuredBuffer<uint> stats = ResourceDescriptorHeap[gStatsBuffer];
    stats[index] = value;
#endif
}

// ------------------------------

[numthreads(8, 8, 1)]
void CS_Clear(uint3 dtId : SV_DispatchThreadID)
{
    RWTexture2D<uint> pageTable = ResourceDescriptorHeap[gPageTable];
    RWTexture2D<uint> pageRequest = ResourceDescriptorHeap[gPageRequest];

    pageTable[dtId.xy] = 0;
    pageRequest[dtId.xy] = 0;

    if (all(dtId.xy == 0))
    {
        RWStructuredBuffer<int> lists = ResourceDescriptorHeap[gCurList];
        RWStructuredBuffer<uint> renderCount = ResourceDescriptorHeap[gRenderCount];
        RWStructuredBuffer<uint4> uncachedBounds = ResourceDescriptorHeap[gUncachedBounds];

        lists[ListCounter(LIST_LRU)] = VSM_MAX_PHYS_PAGES;
        lists[ListCounter(LIST_AVAILABLE)] = 0;
        lists[ListCounter(LIST_EMPTY)] = 0;
        lists[ListCounter(LIST_REQUESTED)] = 0;
        renderCount[0] = 0;

        uint i = 0;
        for (; i < VSM_STAT_NUM; ++i)
            StatsWrite(i, 0);

        for (i = 0; i < VSM_NUM_CLIPMAPS; ++i)
            uncachedBounds[i] = uint4(VSM_PAGES_XY, VSM_PAGES_XY, 0, 0);
    }
}

[numthreads(32, 1, 1)]
void CS_RemapPhysicalPages(uint3 dtId : SV_DispatchThreadID)
{
    uint slot = dtId.x;

    if (slot >= VSM_MAX_PHYS_PAGES)
        return;

    RWStructuredBuffer<PhysMetaEntry> physMeta = ResourceDescriptorHeap[gPhysMeta];
    PhysMetaEntry m = physMeta[slot];

    if (!(m.Flags & PM_ALLOCATED))
        return;

    RWStructuredBuffer<ClipmapLevelGPU> levels = ResourceDescriptorHeap[gClipmapBuffer];
    if (gInvalidateAll || m.LevelIndex >= VSM_NUM_CLIPMAPS)
    {
        physMeta[slot].Flags = 0;
        return;
    }

    int2 a = int2(m.PageAddr) + levels[m.LevelIndex].PageOffset;

    if (any(a < 0) || any(a >= VSM_PAGES_XY) || gInvalidateAll)
        physMeta[slot].Flags = 0; // cached data is gone
    else
        physMeta[slot].PageAddr = uint2(a); // page panned with the window
}

[numthreads(8, 8, 1)]
void CS_Mark(uint3 dtId : SV_DispatchThreadID)
{
    if (any(dtId.xy >= gScreenSize))
        return;

    Texture2D<float> depthTex = ResourceDescriptorHeap[gSSDepthTexture];
    float depth = depthTex.Load(int3(dtId.xy, 0));

    if (depth <= 0.0)
        return;

    RWTexture2D<uint> pageRequest = ResourceDescriptorHeap[gPageRequest];

    float2 texUV = (dtId.xy + 0.5) / gScreenSize;
    float3 worldPos = ReconstructWorldPosFromDepth(depth, texUV, gInvProj, gInvView).xyz;
    float3 lookupPos = VSM_OffsetReceiver(worldPos, VSM_LoadWorldNormal(dtId.xy));
    VSMParams p = VSM_GetParams(lookupPos);
    VSMAddress address = VSM_WorldToVirtual(p, lookupPos);
    if (!address.InWindow)
        return;

    // The directional screen ray changes only comparison depth, not this virtual address.
    pageRequest[VSM_PageTableAddress(address.Page, p.LevelIndex)] = 1;
}

[numthreads(32, 1, 1)]
void CS_UpdatePhysicalPages(uint3 dtId : SV_DispatchThreadID)
{
    uint listIndex = dtId.x;

    if (listIndex >= VSM_MAX_PHYS_PAGES)
        return;

    RWStructuredBuffer<int> prevLists = ResourceDescriptorHeap[gPrevList];
    RWStructuredBuffer<int> lists = ResourceDescriptorHeap[gCurList];
    RWStructuredBuffer<PhysMetaEntry> physMeta = ResourceDescriptorHeap[gPhysMeta];
    RWTexture2D<uint> pageRequest = ResourceDescriptorHeap[gPageRequest];
    RWTexture2D<uint> pageTable = ResourceDescriptorHeap[gPageTable];

    // Most recently requested pages come first in last frame's REQUESTED list
    uint slot = gHasHistory ? prevLists[ListStart(LIST_REQUESTED) + listIndex] : listIndex;

    PhysMetaEntry m = physMeta[slot];
    uint nextFlags = 0;
    bool removed = false;
    bool requested = false;
    bool keptUnrequested = false;

    if (m.Flags & PM_ALLOCATED)
    {
        if (pageRequest[VSM_PageTableAddress(m.PageAddr, m.LevelIndex)] != 0)
        {
            // Cached hit: map it, no need to render
            pageTable[VSM_PageTableAddress(m.PageAddr, m.LevelIndex)] = PT_REQUESTED | PT_MAPPED | slot;
            physMeta[slot].LastRequestedFrame = gFrame;
            PushList(lists, LIST_REQUESTED, slot);
            removed = true;
            nextFlags = PM_ALLOCATED;
            requested = true;
        }
        else if (gFrame - m.LastRequestedFrame <= VSM_MAX_PAGE_AGE)
        {
            // Keep the data but do not map it: it stays in LRU as an eviction candidate
            nextFlags = PM_ALLOCATED;
            keptUnrequested = true;
        }
    }

    if (nextFlags == 0)
    {
        PushList(lists, LIST_EMPTY, slot);
        removed = true;
    }

    physMeta[slot].Flags = nextFlags;
    lists[ListStart(LIST_LRU) + listIndex] = removed ? -1 : slot;

    StatsInc(VSM_STAT_CACHED_HITS, requested);
    StatsInc(VSM_STAT_CACHED_UNREQUESTED, keptUnrequested);
    StatsInc(VSM_STAT_EMPTY_PAGES, nextFlags == 0);
}

groupshared uint gsWaveSums[PACK_GROUP_SIZE / 4]; // 4 = minimum wave size allowed by the spec

// Exclusive prefix sum over the whole group; 'total' receives the group sum
uint GroupPrefixSum(uint value, uint gi, out uint total)
{
    const uint laneCount = WaveGetLaneCount();
    const uint waveIndex = gi / laneCount;
    const uint numWaves = PACK_GROUP_SIZE / laneCount;

    uint prefix = WavePrefixSum(value);
    uint waveSum = WaveActiveSum(value);

    if (WaveIsFirstLane())
        gsWaveSums[waveIndex] = waveSum;

    GroupMemoryBarrierWithGroupSync();

    total = 0;
    for (uint w = 0; w < numWaves; ++w)
    {
        uint s = gsWaveSums[w];
        prefix += (w < waveIndex) ? s : 0;
        total += s;
    }

    GroupMemoryBarrierWithGroupSync(); // gsWaveSums is reused by the next iteration
    return prefix;
}

[numthreads(PACK_GROUP_SIZE, 1, 1)]
void CS_PackAvailablePages(uint gi : SV_GroupIndex)
{
    RWStructuredBuffer<int> lists = ResourceDescriptorHeap[gCurList];

    uint total = 0;
    // Uniform loop: every thread must reach the barriers in GroupPrefixSum
    for (uint base = 0; base < VSM_MAX_PHYS_PAGES; base += PACK_GROUP_SIZE)
    {
        uint i = base + gi;
        int slot = (i < VSM_MAX_PHYS_PAGES) ? lists[ListStart(LIST_LRU) + i] : -1;

        uint chunkTotal = 0;
        uint offset = GroupPrefixSum(slot >= 0 ? 1u : 0u, gi, chunkTotal);

        if (slot >= 0)
            lists[ListStart(LIST_AVAILABLE) + total + offset] = slot;

        total += chunkTotal;
    }

    // Empty pages go last: allocation pops from the end, so they are used before cached ones
    uint emptyCount = lists[ListCounter(LIST_EMPTY)];
    for (uint j = gi; j < emptyCount; j += PACK_GROUP_SIZE)
        lists[ListStart(LIST_AVAILABLE) + total + j] = lists[ListStart(LIST_EMPTY) + j];

    if (gi == 0)
    {
        lists[ListCounter(LIST_AVAILABLE)] = total + emptyCount;
        StatsWrite(VSM_STAT_AVAILABLE_AFTER_PACK, total + emptyCount);
    }
}

[numthreads(8, 8, 1)]
void CS_Allocate(uint3 dtId : SV_DispatchThreadID)
{
    RWTexture2D<uint> pageRequest = ResourceDescriptorHeap[gPageRequest];
    RWTexture2D<uint> pageTable = ResourceDescriptorHeap[gPageTable];

    bool requested = pageRequest[dtId.xy] != 0;
    bool needAlloc = false;
    if (requested)
        needAlloc = !(pageTable[dtId.xy] & PT_MAPPED);

    int slot = -1;
    bool evicted = false;

    if (needAlloc)
    {
        RWStructuredBuffer<int> lists = ResourceDescriptorHeap[gCurList];
        slot = PopList(lists, LIST_AVAILABLE);

        if (slot < 0)
        {
            pageTable[dtId.xy] = PT_REQUESTED | PT_OVERFLOW;
        }
        else
        {
            RWStructuredBuffer<PhysMetaEntry> physMeta = ResourceDescriptorHeap[gPhysMeta];

            // Evicting a cached page: it is unrequested, so it is not in the page table and no
            // other thread touches it. UE still clears its entry defensively (AllocateNewPageMappings)
            PhysMetaEntry old = physMeta[slot];
            evicted = (old.Flags & PM_ALLOCATED) != 0;
            if (evicted)
                pageTable[VSM_PageTableAddress(old.PageAddr, old.LevelIndex)] = 0;

            physMeta[slot].Flags = PM_ALLOCATED;
            physMeta[slot].PageAddr = uint2(dtId.x, dtId.y % VSM_PAGES_XY);
            physMeta[slot].LevelIndex = dtId.y / VSM_PAGES_XY;
            physMeta[slot].AllocFrame = gFrame;
            physMeta[slot].LastRequestedFrame = gFrame;

            pageTable[dtId.xy] = PT_REQUESTED | PT_MAPPED | PT_RENDER | slot;
            PushList(lists, LIST_REQUESTED, slot);

            RWStructuredBuffer<RenderListEntry> renderList = ResourceDescriptorHeap[gRenderList];
            RWStructuredBuffer<uint> renderCount = ResourceDescriptorHeap[gRenderCount];
            RWStructuredBuffer<uint4> uncachedBounds = ResourceDescriptorHeap[gUncachedBounds];

            uint r;
            InterlockedAdd(renderCount[0], 1, r);

            renderList[r].Slot = slot;

            uint2 pageAddr = uint2(dtId.x, dtId.y % VSM_PAGES_XY);
            uint levelIndex = dtId.y / VSM_PAGES_XY;

            InterlockedMin(uncachedBounds[levelIndex].x, pageAddr.x);
            InterlockedMin(uncachedBounds[levelIndex].y, pageAddr.y);
            InterlockedMax(uncachedBounds[levelIndex].z, pageAddr.x + 1);
            InterlockedMax(uncachedBounds[levelIndex].w, pageAddr.y + 1);
        }
    }

    StatsInc(VSM_STAT_REQUESTED_PAGES, requested);
    StatsInc(VSM_STAT_ALLOCATED_NEW, slot >= 0);
    StatsInc(VSM_STAT_EVICTED_CACHED, evicted);
    StatsInc(VSM_STAT_OVERFLOW, needAlloc && slot < 0);
}

[numthreads(PACK_GROUP_SIZE, 1, 1)]
void CS_FinalizeLists(uint gi : SV_GroupIndex)
{
    RWStructuredBuffer<int> lists = ResourceDescriptorHeap[gCurList];

    int available = max(lists[ListCounter(LIST_AVAILABLE)], 0); // can be negative after overflow pops
    int requested = lists[ListCounter(LIST_REQUESTED)];
    int copyCount = min(available, int(VSM_MAX_PHYS_PAGES) - requested);

    for (int i = gi; i < copyCount; i += PACK_GROUP_SIZE)
        lists[ListStart(LIST_REQUESTED) + requested + i] = lists[ListStart(LIST_AVAILABLE) + i];

    // All threads read the counters before thread 0 rewrites them
    AllMemoryBarrierWithGroupSync();

    if (gi == 0)
    {
        lists[ListCounter(LIST_REQUESTED)] = requested + copyCount; // invariant: == VSM_MAX_PHYS_PAGES
        lists[ListCounter(LIST_AVAILABLE)] = 0;
        StatsWrite(VSM_STAT_REQUESTED_LIST_FINAL, requested + copyCount);
    }
}

groupshared uint gsSeen[VSM_MAX_PHYS_PAGES / 32];

[numthreads(PACK_GROUP_SIZE, 1, 1)]
void CS_ValidateLists(uint gi : SV_GroupIndex)
{
    RWStructuredBuffer<int> lists = ResourceDescriptorHeap[gCurList];

    for (uint w = gi; w < VSM_MAX_PHYS_PAGES / 32; w += PACK_GROUP_SIZE)
        gsSeen[w] = 0;

    GroupMemoryBarrierWithGroupSync();

    uint count = min((uint) lists[ListCounter(LIST_REQUESTED)], VSM_MAX_PHYS_PAGES);
    for (uint i = gi; i < count; i += PACK_GROUP_SIZE)
    {
        uint slot = lists[ListStart(LIST_REQUESTED) + i];
        bool valid = slot < VSM_MAX_PHYS_PAGES;
        uint prev = 0;
        if (valid)
            InterlockedOr(gsSeen[slot >> 5], 1u << (slot & 31), prev);
        StatsInc(VSM_STAT_LIST_DUPLICATES, !valid || (prev & (1u << (slot & 31))) != 0);
    }
}

[numthreads(1, 1, 1)]
void CS_DispatchArgs(uint3 id : SV_DispatchThreadID)
{
    RWStructuredBuffer<uint> renderCount = ResourceDescriptorHeap[gRenderCount];
    RWStructuredBuffer<uint> clearDispatchArgs = ResourceDescriptorHeap[gClearDispatchArgs];

    clearDispatchArgs[0] = 4;               // [0] ThreadGroupCountX;
    clearDispatchArgs[1] = 4;               // [1] ThreadGroupCountY;
    clearDispatchArgs[2] = renderCount[0];  // [2] ThreadGroupCountZ;
}

[numthreads(16, 16, 1)]
void CS_ClearSlots(uint3 gId : SV_GroupID, uint3 gtId : SV_GroupThreadID)
{
    RWStructuredBuffer<RenderListEntry> renderList = ResourceDescriptorHeap[gRenderList];
    RWTexture2D<uint> physPool = ResourceDescriptorHeap[gPhysPool];

    // One page (128x128) splits into 16 tiles (32x32)
    // One thread group clears one tile (32x32)

    uint pageIndex = gId.z;
    uint2 tile = gId.xy;

    uint slot = renderList[pageIndex].Slot;
    uint2 physPage = VSM_SlotToPhysPage(slot);

    uint2 base = physPage * VSM_PAGE_SIZE + tile * 32 + gtId.xy;

    physPool[base]                  = 0u;
    physPool[base + uint2(16, 0)]   = 0u;
    physPool[base + uint2(0, 16)]   = 0u;
    physPool[base + uint2(16, 16)]  = 0u;
}

// -------------------- VS --------------------

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float3 TangentU : TANGENT;
    float2 TexC : TEXCOORD;
};

struct VSOut_G
{
    float4 Pos : SV_Position;
    float4 ClipD : SV_ClipDistance;
    nointerpolation uint Level : TEXCOORD0;
};

VSOut_G VS_Geometry(VertexIn vIn, uint inst : SV_InstanceID)
{
    RWStructuredBuffer<ClipmapLevelGPU> levels = ResourceDescriptorHeap[gClipmapBuffer];
    RWStructuredBuffer<uint4> uncachedBounds = ResourceDescriptorHeap[gUncachedBounds];

    float4 p = mul(mul(float4(vIn.PosL, 1), gWorld), levels[inst].WorldToShadowClip);

    // ndc = p.xy / p.w
    // uv = ndc * 0.5 + 0.5 = p.xy * 0.5 / p.w + 0.5 = (p.xy * 0.5 + 0.5 * p.w) / p.w
    float2 virtualPixelH = (p.xy * float2(0.5, -0.5) + 0.5 * p.w) * VSM_VIRTUAL_SIZE;

    float2 minPixel = float2(uncachedBounds[inst].xy) * VSM_PAGE_SIZE;
    float2 maxPixel = float2(uncachedBounds[inst].zw) * VSM_PAGE_SIZE;

    VSOut_G o;

    // vp = uv * VSM, vp' = vp * p.w
    // vp >= minPixel (*p.w) => vp' >= minPixel * p.w => vp - minPixel * p.w >= 0
    // vp < maxPixel  (*p.w) => vp' < maxPixel * p.w  => maxPixel * p.w - vp > 0
    o.ClipD = float4(
        virtualPixelH - minPixel * p.w,
        maxPixel * p.w - virtualPixelH);

    o.Pos = p;
    o.Level = inst;

    return o;
}

// -------------------- PS --------------------

struct VertexOut // From FSQuadVS.hlsl
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);

void PS_Geometry(VSOut_G pin)
{
    RWTexture2D<uint> physPool = ResourceDescriptorHeap[gPhysPool];
    RWTexture2D<uint> pageTable = ResourceDescriptorHeap[gPageTable];

    uint2 virtualTexel = (uint2)pin.Pos.xy;

    uint2 virtualPage = virtualTexel >> VSM_LOG2_PAGE_SIZE;

    uint entry = pageTable[VSM_PageTableAddress(virtualPage, pin.Level)];

    if ((entry & PT_RENDER) == 0)
        return;

    uint slot = entry & PT_SLOT_MASK;
    uint2 physicalPage = VSM_SlotToPhysPage(slot);

    uint2 physicalTexel = physicalPage * VSM_PAGE_SIZE + (virtualTexel & (VSM_PAGE_SIZE - 1));

    InterlockedMax(
        physPool[physicalTexel],
        asuint(pin.Pos.z)
    );
}

float PS_DrawShadow(VertexOut pin) : SV_Target
{
    uint2 pixel = uint2(pin.PosH.xy);
    Texture2D<float> depthTex = ResourceDescriptorHeap[gSSDepthTexture];
    float depth = depthTex.Load(int3(pixel, 0));
    if (depth <= 0.0)
        return 1;

    float2 uv = (float2(pixel) + 0.5f) / gScreenSize;
    float3 worldPos = ReconstructWorldPosFromDepth(depth, uv, gInvProj, gInvView).xyz;
    float3 normal = VSM_LoadWorldNormal(pixel);
    float3 lookupPos = VSM_OffsetReceiver(worldPos, normal);
    VSMParams p = VSM_GetParams(lookupPos);
    float rayStartDistance = VSM_GetRayStartDistance(worldPos, lookupPos, pixel);
    return VSM_ShadowFactor(p, lookupPos, normal, rayStartDistance, gMaxSlopeOffsetWorld);
}

float4 PS_DrawDebug(VertexOut pin) : SV_Target
{
    Texture2D depthTex = ResourceDescriptorHeap[gSSDepthTexture];

    float depth = depthTex.Sample(gsamPointClamp, pin.TexC).r;

    if (depth <= 0.0)
        return 0;

    float3 worldPos = ReconstructWorldPosFromDepth(depth, pin.TexC, gInvProj, gInvView).xyz;
    float3 lookupPos = VSM_OffsetReceiver(worldPos, VSM_LoadWorldNormal(uint2(pin.PosH.xy)));
    VSMParams p = VSM_GetParams(lookupPos);

    RWStructuredBuffer<ClipmapLevelGPU> levels = ResourceDescriptorHeap[gClipmapBuffer];
    VSMAddress a = VSM_WorldToVirtual(p, lookupPos);

    if (!a.InWindow)
        return 0;

    float4 color = 0;

    if (gDebugMode == 1)
    {
        color = float4(VSM_DebugPageGrid(p, a, levels[p.LevelIndex].WindowOriginPage), 1);
    }
    else if (gDebugMode == 2)
    {
        VSMSample s = VSM_Sample(p, lookupPos);
        color = float4(VSM_DebugCacheState(s, gFrame, gPhysMeta), 1);
    }

    if (gDebugMode == 3)
    {
        const float3 palette[4] = {
            float3(0.9, 0.2, 0.15), float3(0.2, 0.8, 0.25),
            float3(0.15, 0.45, 1.0), float3(1.0, 0.75, 0.1)
        };
        color = float4(palette[p.LevelIndex % 4], 1);
    }
    return color;
}
