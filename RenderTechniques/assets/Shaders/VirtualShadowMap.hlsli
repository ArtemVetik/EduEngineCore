#ifndef VIRTUAL_SHADOW_MAP_HLSLI
#define VIRTUAL_SHADOW_MAP_HLSLI

#define VSM_PAGE_SIZE       128
#define VSM_LOG2_PAGE_SIZE  7
#define VSM_PAGES_XY        128
#define VSM_VIRTUAL_SIZE    (VSM_PAGE_SIZE * VSM_PAGES_XY)
#define VSM_POOL_SIZE       64

#define PM_ALLOCATED 0x1

#define PT_MAPPED     (1u << 31)   // page has slot (from cache or new)
#define PT_REQUESTED  (1u << 30)   // page is visible in this frame (Mark)
#define PT_OVERFLOW   (1u << 29)   // page is needed, but there were no available slots
#define PT_SLOT_MASK  0xFFFFu

struct PhysMetaEntry
{
    uint Flags;
    uint LastUsedFrame;
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
    s.Entry = pageTable.Load(int3(a.Page, 0));
    
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