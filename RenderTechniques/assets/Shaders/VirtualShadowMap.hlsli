#ifndef VIRTUAL_SHADOW_MAP_HLSLI
#define VIRTUAL_SHADOW_MAP_HLSLI

#define VSM_PAGE_SIZE       128
#define VSM_LOG2_PAGE_SIZE  7
#define VSM_PAGES_XY        128
#define VSM_VIRTUAL_SIZE    (VSM_PAGE_SIZE * VSM_PAGES_XY)
#define VSM_POOL_SIZE       64
#define VSM_MAX_PHYS_PAGES  (VSM_POOL_SIZE * VSM_POOL_SIZE)
#define VSM_MAX_PAGE_AGE    1000

#define PM_ALLOCATED 0x1

#define PT_MAPPED     (1u << 31)   // page has slot (from cache or new)
#define PT_REQUESTED  (1u << 30)   // page is visible in this frame (Mark)
#define PT_OVERFLOW   (1u << 29)   // page is needed, but there were no available slots
#define PT_SLOT_MASK  0xFFFFu

#define LIST_LRU        0 // last frame's order, requested/empty entries replaced by -1
#define LIST_AVAILABLE  1 // packed LRU + empty at the end; allocation pops from the end. [ LRU...LRU, EMPTY...EMPTY ]
#define LIST_EMPTY      2 // invalid pages, appended to AVAILABLE after packing
#define LIST_REQUESTED  3 // pages used this frame; becomes next frame's LRU order

struct PhysMetaEntry
{
    uint Flags;
    uint LastRequestedFrame;
    uint AllocFrame;
    uint2 PageAddr;
};

struct VSMParams
{
    float4x4 LightViewProj; // [0,1]^2
    uint PageTable;
    uint PhysPool;
    float Bias;
};

struct VSMAddress
{
    bool InWindow;
    uint2 VirtualTexel; // 0..(VSM_SIZE-1)
    uint2 Page;         // 0..(VSM_SIZE/PAGE_SIZE-1)
    float Depth;
};

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
    int offset;
    InterlockedAdd(lists[ListCounter(list)], 1, offset);
    
    // Overflow here means a physical page is lost forever - must never happen
    // If that happened, it means something went wrong before that
    if (offset >= VSM_MAX_PHYS_PAGES)
        return false;
    
    lists[ListStart(list) + offset] = slot;
    return true;
}

int PopList(RWStructuredBuffer<int> lists, uint list)
{
    int offset;
    InterlockedAdd(lists[ListCounter(list)], -1, offset);
    offset -= 1; // index after decrement; counter may go negative on overflow
    return offset < 0 ? -1 : lists[ListStart(list) + offset];
}

VSMAddress VSM_WorldToVirtual(VSMParams p, float3 worldPos)
{
    VSMAddress a;
    float3 uvz = mul(float4(worldPos, 1), p.LightViewProj).xyz;
    a.InWindow = all(uvz.xy >= 0) && all(uvz.xy < 1);
    a.VirtualTexel = uint2(uvz.xy * VSM_VIRTUAL_SIZE);
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

// ---------- Sampling ----------

struct VSMSample
{
    bool Valid;
    uint Entry;
    float OccluderDepth;
    float ReceiverDepth;
};

VSMSample VSM_Sample(VSMParams p, float3 worldPos)
{
    VSMSample s = (VSMSample) 0;
    VSMAddress a = VSM_WorldToVirtual(p, worldPos);
    s.ReceiverDepth = a.Depth;
    
    if (!a.InWindow)
        return s;

    RWTexture2D<uint> pageTable = ResourceDescriptorHeap[p.PageTable]; // TODO: pageTable now is UAV. Create SRV
    s.Entry = pageTable[a.Page];
    
    if (!(s.Entry & PT_MAPPED))
        return s;

    Texture2D<float> pool = ResourceDescriptorHeap[p.PhysPool];
    s.OccluderDepth = pool.Load(int3(VSM_SlotToPhysTexel(p, s.Entry & PT_SLOT_MASK, a.VirtualTexel), 0));
    s.Valid = true;
    
    return s;
}

float VSM_ShadowFactor(VSMParams p, float3 worldPos)
{
    VSMSample s = VSM_Sample(p, worldPos);
    
    if (!s.Valid)
        return 1.0;
    
    return (s.ReceiverDepth + p.Bias >= s.OccluderDepth) ? 1.0 : 0.0; // reverse-Z
}

#endif // VIRTUAL_SHADOW_MAP_HLSLI