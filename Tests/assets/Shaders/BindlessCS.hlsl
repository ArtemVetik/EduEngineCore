// Used by ShaderResourcesTests: resources reached through ResourceDescriptorHeap,
// the way the engine does bindless rendering.

cbuffer cbIndices : register(b0)
{
    uint gAlbedoIdx;
    uint gOutputIdx;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    Texture2D<float4> albedo = ResourceDescriptorHeap[gAlbedoIdx];
    RWTexture2D<float4> output = ResourceDescriptorHeap[gOutputIdx];

    output[id.xy] = albedo.Load(int3(id.xy, 0));
}
