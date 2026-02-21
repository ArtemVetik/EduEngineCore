
cbuffer cbPass : register(b0)
{
    uint gMetalRoughnessTexIdx;
    uint gAOTexIdx;
    uint gOutORMTexIdx;
    uint gMipLevel;
    uint2 gTexSize;
};

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= gTexSize.x || id.y >= gTexSize.y)
        return;
    
    Texture2D<float4> metalRoughnessTex = ResourceDescriptorHeap[gMetalRoughnessTexIdx];
    Texture2D<float4> aoTex = ResourceDescriptorHeap[gAOTexIdx];
    RWTexture2D<float4> outORMTex = ResourceDescriptorHeap[gOutORMTexIdx];
    
    float4 mr = metalRoughnessTex.Load(int3(id.xy, gMipLevel));
    float4 ao = aoTex.Load(int3(id.xy, gMipLevel));

    float metallic = mr.b;
    float roughness = mr.g;
    float ambient = ao.r;

    outORMTex[id.xy] = float4(ambient, roughness, metallic, 1.0);
}
