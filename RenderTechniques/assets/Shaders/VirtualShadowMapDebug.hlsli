#include "VirtualShadowMap.hlsli"

float3 HashColor(uint id)
{
    uint h = id * 747796405u + 2891336453u;
    h = ((h >> ((h >> 28u) + 4u)) ^ h) * 277803737u;
    h = (h >> 22u) ^ h;

    return float3(
        (h & 0xFFu),
        ((h >> 8u) & 0xFFu),
        ((h >> 16u) & 0xFFu)
    ) / 255.0;
}

float3 VSM_DebugCacheState(VSMSample s, float3 baseColor, uint frame, uint physMetaBuff)
{
    float3 c;
    
    if (s.Entry & PT_OVERFLOW)
    {
        c = float3(1, 0, 1); // Purple: Overflow
    }
    else if (s.Entry & PT_MAPPED)
    {
        RWStructuredBuffer<PhysMetaEntry> physMeta = ResourceDescriptorHeap[physMetaBuff]; // TODO: create SRV index
        
        uint slot = s.Entry & PT_SLOT_MASK;
        uint age = frame - physMeta[slot].AllocFrame;
        float t = saturate(age / 60.0);
        c = lerp(float3(1, 0, 0), float3(0, 1, 0), t);
    }
    else
    {
        c = float3(1, 1, 0); // Yellow: Bug
    }
    
    if (!(s.Entry & PT_REQUESTED) && (s.Entry & PT_MAPPED))
        c = float3(1, 1, 1); // White: Bug

    if (!s.Valid)
        c = 0;

    return lerp(baseColor, c, 0.5);
}

float3 VSM_DebugPageGrid(VSMParams p, VSMAddress a, int2 windowOrigin)
{
    int2 absPage = a.Page + windowOrigin;
    float3 c = HashColor(absPage.y * VSM_PAGES_XY + absPage.x);
    
    if (any((a.VirtualTexel & (VSM_PAGE_SIZE - 1)) == 0))
        c = 0; // edge
        
    return c;
}