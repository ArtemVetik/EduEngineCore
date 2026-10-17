#include "CommonTransforms.hlsli"
#include "VirtualShadowMap.hlsli"

#define PM_LIVE_FRAME 60

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
}

cbuffer cbBuffers : register(b2)
{
    uint gPhysPool;
    uint gPhysMeta;
    uint gPageTable;
    uint gFreeList;
    uint gRenderList;
    uint gRenderCount;
    uint gAllocCount;
    uint gSSDepthTexture;
    uint gQuadDrawArgs;
    uint gDrawArgs;
    uint gDrawCount;
}

[numthreads(8, 8, 1)]
void CS_Clear(uint3 dtId : SV_DispatchThreadID)
{
    RWTexture2D<uint> pageTable = ResourceDescriptorHeap[gPageTable];
    
    pageTable[dtId.xy] = 0;
    
    if (!all(dtId.xy))
    {
        RWStructuredBuffer<uint> freeList = ResourceDescriptorHeap[gFreeList];
        RWStructuredBuffer<uint> renderCount = ResourceDescriptorHeap[gRenderCount];
        RWStructuredBuffer<uint> allocCount = ResourceDescriptorHeap[gAllocCount];
        
        freeList[0] = 0;
        renderCount[0] = 0;
        allocCount[0] = 0;
    }
}

[numthreads(32, 1, 1)]
void CS_UpdatePhysicalPages(uint3 dtId : SV_DispatchThreadID)
{
    RWStructuredBuffer<PhysMetaEntry> physMeta = ResourceDescriptorHeap[gPhysMeta];
    RWTexture2D<uint> pageTable = ResourceDescriptorHeap[gPageTable];
    
    uint slot = dtId.x;
    
    PhysMetaEntry m = physMeta[slot];
    bool keep = false;
    
    if ((m.Flags & PM_ALLOCATED) && !gInvalidateAll)
    {
        int2 a = int2(m.PageAddr) + gPageOffset;
        if (all(a >= 0) && all(a < VSM_PAGES_XY) && gFrame - m.LastUsedFrame <= PM_LIVE_FRAME)
        {
            physMeta[slot].PageAddr = a;
            pageTable[a] = PT_MAPPED | slot; // restore entry from cache
            keep = true;
        }
    }
    
    if (!keep)
    {
        RWStructuredBuffer<uint> freeList = ResourceDescriptorHeap[gFreeList];
        
        physMeta[slot].Flags = 0;
        uint i;
        InterlockedAdd(freeList[0], 1, i); // freeList[0] === freeCount
        freeList[i + 1] = slot;
    }
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
    
    RWTexture2D<uint> pageTable = ResourceDescriptorHeap[gPageTable];
    
    float2 texUV = (dtId.xy + 0.5) / gScreenSize;
    float4 worldPos = ReconstructWorldPosFromDepth(depth, texUV, gInvProj, gInvView);
    float2 uv = mul(float4(worldPos.xyz, 1.0f), gLightViewProj).xy;
    
    if (any(uv < 0) || any(uv >= 1))
        return;
    
    InterlockedOr(pageTable[uint2(uv * VSM_VIRTUAL_SIZE) >> VSM_LOG2_PAGE_SIZE], PT_REQUESTED);
}

[numthreads(8, 8, 1)]
void CS_Allocate(uint3 dtId : SV_DispatchThreadID)
{
    RWTexture2D<uint> pageTable = ResourceDescriptorHeap[gPageTable];
    
    uint tableEntry = pageTable[dtId.xy];
    
    if (!(tableEntry & PT_REQUESTED))
        return;

    RWStructuredBuffer<PhysMetaEntry> physMeta = ResourceDescriptorHeap[gPhysMeta];
    
    if (tableEntry & PT_MAPPED)
    {
        physMeta[tableEntry & PT_SLOT_MASK].LastUsedFrame = gFrame;
        return;
    }
    
    RWStructuredBuffer<uint> freeList = ResourceDescriptorHeap[gFreeList];
    RWStructuredBuffer<uint> allocCount = ResourceDescriptorHeap[gAllocCount];
    
    uint i;
    InterlockedAdd(allocCount[0], 1, i);
    
    if (i >= freeList[0])
    {
        pageTable[dtId.xy] = tableEntry | PT_OVERFLOW;
        return;
    }
    
    uint slot = freeList[i + 1];
    
    physMeta[slot].Flags = PM_ALLOCATED;
    physMeta[slot].PageAddr = dtId.xy;
    physMeta[slot].LastUsedFrame = gFrame;
    physMeta[slot].AllocFrame = gFrame;
    
    pageTable[dtId.xy] = PT_MAPPED | PT_REQUESTED | slot;
    
    RWStructuredBuffer<RenderListEntry> renderList = ResourceDescriptorHeap[gRenderList];
    RWStructuredBuffer<uint> renderCount = ResourceDescriptorHeap[gRenderCount];
    
    uint r;
    InterlockedAdd(renderCount[0], 1, r);
    
    renderList[r].Slot = slot;
    renderList[r].PageAddr = dtId.xy;
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