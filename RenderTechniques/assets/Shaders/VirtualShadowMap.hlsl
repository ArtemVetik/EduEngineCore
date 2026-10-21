#include "CommonTransforms.hlsli"
#include "VirtualShadowMap.hlsli"
#include "VirtualShadowMapDebug.hlsli"

#define PACK_GROUP_SIZE 1024

groupshared uint gsWaveSums[PACK_GROUP_SIZE / 4]; // 4 = minimum wave size allowed by the spec

struct RenderListEntry
{
    uint Slot;
    uint2 PageAddr;
};

cbuffer cbObject : register(b0)
{
    float4x4 gWorld;
}

cbuffer cbPass : register(b1)
{
    float4x4 gInvView;
    float4x4 gInvProj;
    float4x4 gLightViewProj;
    float4x4 gLightViewProjNDC;
    uint2 gScreenSize;
    int2 gPageOffset;
    uint gFrame;
    uint gInvalidateAll;
    int2 gWindowOriginPage;
    uint gCurList;  // UAV heap undex
    uint gPrevList; // UAV heap undex
    uint gHasHistory;
    uint gDebugMode;
}

cbuffer cbBuffers : register(b2)
{
    uint gPhysMeta;
    uint gPageRequest;
    uint gPageTable;
    uint gRenderList;
    uint gRenderCount;
    uint gSSDepthTexture;
    uint gQuadDrawArgs;
    uint gDrawArgs;
    uint gDrawCount;
    uint gPhysPool;
}

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
        
        lists[ListCounter(LIST_LRU)] = VSM_MAX_PHYS_PAGES;
        lists[ListCounter(LIST_AVAILABLE)] = 0;
        lists[ListCounter(LIST_EMPTY)] = 0;
        lists[ListCounter(LIST_REQUESTED)] = 0;
        renderCount[0] = 0;
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
    
    int2 a = int2(m.PageAddr) + gPageOffset;
    
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
    float4 worldPos = ReconstructWorldPosFromDepth(depth, texUV, gInvProj, gInvView);
    float2 uv = mul(float4(worldPos.xyz, 1.0f), gLightViewProj).xy;
    
    if (any(uv < 0) || any(uv >= 1))
        return;
    
    pageRequest[uint2(uv * VSM_VIRTUAL_SIZE) >> VSM_LOG2_PAGE_SIZE] = 1;
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
    
    if (m.Flags & PM_ALLOCATED)
    {
        if (pageRequest[m.PageAddr] != 0)
        {
            // Cached hit: map it, no need to render
            pageTable[m.PageAddr] = PT_REQUESTED | PT_MAPPED | slot;
            physMeta[slot].LastRequestedFrame = gFrame;
            PushList(lists, LIST_REQUESTED, slot);
            removed = true;
            nextFlags = PM_ALLOCATED;
        }
        else if (gFrame - m.LastRequestedFrame <= VSM_MAX_PAGE_AGE)
        {
            // Keep the data but do not map it: it stays in LRU as an eviction candidate
            nextFlags = PM_ALLOCATED;
        }
    }
    
    if (nextFlags == 0)
    {
        PushList(lists, LIST_EMPTY, slot);
        removed = true;
    }
    
    physMeta[slot].Flags = nextFlags;
    lists[ListStart(LIST_LRU) + listIndex] = removed ? -1 : slot;
}

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
        lists[ListCounter(LIST_AVAILABLE)] = total + emptyCount;
}

[numthreads(8, 8, 1)]
void CS_Allocate(uint3 dtId : SV_DispatchThreadID)
{
    RWTexture2D<uint> pageRequest = ResourceDescriptorHeap[gPageRequest];
    RWTexture2D<uint> pageTable = ResourceDescriptorHeap[gPageTable];
    
    if (pageRequest[dtId.xy] == 0)
        return;
    
    if (pageTable[dtId.xy] & PT_MAPPED)
        return;
    
    RWStructuredBuffer<int> lists = ResourceDescriptorHeap[gCurList];
    int slot = PopList(lists, LIST_AVAILABLE);
    
    if (slot < 0)
    {
        pageTable[dtId.xy] = PT_REQUESTED | PT_OVERFLOW; // now means "really out of pages"
        return;
    }
    
    RWStructuredBuffer<PhysMetaEntry> physMeta = ResourceDescriptorHeap[gPhysMeta];
    
    // Evicting a cached page: it is unrequested, so it is not in the page table and no
    // other thread touches it. UE still clears its entry defensively (AllocateNewPageMappings)
    PhysMetaEntry old = physMeta[slot];
    if (old.Flags & PM_ALLOCATED)
        pageTable[old.PageAddr] = 0;
    
    physMeta[slot].Flags = PM_ALLOCATED;
    physMeta[slot].PageAddr = dtId.xy;
    physMeta[slot].AllocFrame = gFrame;
    physMeta[slot].LastRequestedFrame = gFrame;
    
    pageTable[dtId.xy] = PT_REQUESTED | PT_MAPPED | slot;
    PushList(lists, LIST_REQUESTED, slot);
    
    RWStructuredBuffer<RenderListEntry> renderList = ResourceDescriptorHeap[gRenderList];
    RWStructuredBuffer<uint> renderCount = ResourceDescriptorHeap[gRenderCount];
    
    uint r;
    InterlockedAdd(renderCount[0], 1, r);
    
    renderList[r].Slot = slot;
    renderList[r].PageAddr = dtId.xy;
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
    }
}

[numthreads(32, 1, 1)]
void CS_DrawArgs(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= gDrawCount)
        return;
    
    RWStructuredBuffer<uint> renderCount = ResourceDescriptorHeap[gRenderCount];
    RWStructuredBuffer<uint> drawArgs = ResourceDescriptorHeap[gDrawArgs];
    
    //[0] IndexCountPerInstance;
    //[1] InstanceCount;
    //[2] StartIndexLocation;
    //[3] BaseVertexLocation;
    //[4] StartInstanceLocation;
    drawArgs[id.x * 5 + 1] = min(renderCount[0], VSM_POOL_SIZE * VSM_POOL_SIZE); // InstanceCount
    
    if (id.x == 0)
    {
        RWStructuredBuffer<uint> quadDrawArgs = ResourceDescriptorHeap[gQuadDrawArgs];
        quadDrawArgs[1] = min(renderCount[0], VSM_POOL_SIZE * VSM_POOL_SIZE); // InstanceCount
    }
}

// -------------------- VS --------------------

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float3 TangentU : TANGENT;
    float2 TexC : TEXCOORD;
};

struct VSOut_CS
{
    float4 Pos : SV_Position;
};

struct VSOut_G
{
    float4 Pos : SV_Position;
    float4 ClipD : SV_ClipDistance;
};

// Topology: Triangle Strip
// Face Cull: None
VSOut_CS VS_ClearSlots(uint vid : SV_VertexID, uint inst : SV_InstanceID)
{
    RWStructuredBuffer<RenderListEntry> renderList = ResourceDescriptorHeap[gRenderList];

    uint slot = renderList[inst].Slot;
    uint2 page = VSM_SlotToPhysPage(slot);
    float2 corner = float2(vid & 1, vid >> 1); // (0,0) (1,0) (0,1) (1,1)
    float2 uv = (page + corner) / VSM_POOL_SIZE;

    VSOut_CS o;
    o.Pos = float4(uv.x * 2 - 1, 1 - uv.y * 2, 0.0, 1.0); // z = 0 (far, reverse-Z)
    return o;
}

VSOut_G VS_Geometry(VertexIn vIn, uint inst : SV_InstanceID)
{
    RWStructuredBuffer<RenderListEntry> renderList = ResourceDescriptorHeap[gRenderList];
    
    RenderListEntry e = renderList[inst];
    
    uint slot = e.Slot;
    
    // 1. World pos -> virtual pixel (0..VSM_VIRTUAL_SIZE)
    float4 p = mul(mul(float4(vIn.PosL, 1), gWorld), gLightViewProjNDC);
    float2 vUV = p.xy * float2(0.5, -0.5) + 0.5; // [-1; 1] -> [0; 1]
    float2 vPix = vUV * VSM_VIRTUAL_SIZE;

    // 2. Pixel pos in page local space
    float2 local = vPix - float2(e.PageAddr) * VSM_PAGE_SIZE;

    VSOut_G o;
    
    // 3. Cull if float4 < 0 (outside page)
    o.ClipD = float4(local, VSM_PAGE_SIZE - local);
    
    // 4. Pixel in physical pool -> NDC
    uint2 physPage = VSM_SlotToPhysPage(slot);
    float2 physPix = float2(physPage) * VSM_PAGE_SIZE + local;
    float2 pUV = physPix / (VSM_POOL_SIZE * VSM_PAGE_SIZE);
    o.Pos = float4(pUV.x * 2 - 1, 1 - pUV.y * 2, p.z, 1);
    
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

float PS_DrawShadow(VertexOut pin) : SV_Target
{
    Texture2D depthTex = ResourceDescriptorHeap[gSSDepthTexture];
    
    float depth = depthTex.Sample(gsamPointClamp, pin.TexC).r;
    
    if (depth <= 0.0)
        return 1;
    
    float4 worldPos = ReconstructWorldPosFromDepth(depth, pin.TexC, gInvProj, gInvView);
    
    VSMParams p;
    p.LightViewProj = gLightViewProj;
    p.PageTable = gPageTable;
    p.PhysPool = gPhysPool;
    p.Bias = 0.0f;
    
    return VSM_ShadowFactor(p, worldPos.xyz);
}

float4 PS_DrawDebug(VertexOut pin) : SV_Target
{
    Texture2D depthTex = ResourceDescriptorHeap[gSSDepthTexture];
    
    float depth = depthTex.Sample(gsamPointClamp, pin.TexC).r;
    
    if (depth <= 0.0)
        return 0;
    
    float4 worldPos = ReconstructWorldPosFromDepth(depth, pin.TexC, gInvProj, gInvView);
    
    VSMParams p;
    p.LightViewProj = gLightViewProj;
    p.PageTable = gPageTable;
    p.PhysPool = gPhysPool;
    p.Bias = 0.0f;
    
    VSMAddress a = VSM_WorldToVirtual(p, worldPos.xyz);
    
    if (!a.InWindow)
        return 0;
    
    float4 color = 0;
    
    if (gDebugMode == 1)
    {
        color = float4(VSM_DebugPageGrid(p, a, gWindowOriginPage), 1);
    }
    else if (gDebugMode == 2)
    {
        VSMSample s = VSM_Sample(p, worldPos.xyz);
        color = float4(VSM_DebugCacheState(s, gFrame, gPhysMeta), 1);
    }
    
    return color;
}