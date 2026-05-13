cbuffer cbConstants : register(b0)
{
    uint gNbCascades;
    uint gTextureSize;
}

cbuffer cbInputTextures : register(b1)
{
    uint gDisplacementsTexturesIdx; // RWTexture2DArray<float3>
    uint gDerivativesTexturesIdx;   // RWTexture2DArray<float4>
    uint gTurbulenceTexturesIdx;    // RWTexture2DArray<float4>
    
    uint gDxDzTexturesIdx;          // RWTexture2DArray<float2>
    uint gDyDxzTexturesIdx;         // RWTexture2DArray<float2>
    uint gDyxDyzTexturesIdx;        // RWTexture2DArray<float2>
    uint gDxxDzzTexturesIdx;        // RWTexture2DArray<float2>
}

[numthreads(8, 8, 1)]
void FillResultTextures(uint3 id : SV_DispatchThreadID)
{
    RWTexture2DArray<float3> displacementsTextures = ResourceDescriptorHeap[gDisplacementsTexturesIdx];
    RWTexture2DArray<float4> derivativesTextures = ResourceDescriptorHeap[gDerivativesTexturesIdx];
    RWTexture2DArray<float4> turbulenceTextures = ResourceDescriptorHeap[gTurbulenceTexturesIdx];
    
    RWTexture2DArray<float2> DxDzTextures = ResourceDescriptorHeap[gDxDzTexturesIdx];
    RWTexture2DArray<float2> DyDxzTextures = ResourceDescriptorHeap[gDyDxzTexturesIdx];
    RWTexture2DArray<float2> DyxDyzTextures = ResourceDescriptorHeap[gDyxDyzTexturesIdx];
    RWTexture2DArray<float2> DxxDzzTextures = ResourceDescriptorHeap[gDxxDzzTexturesIdx];
    
    for (uint i = 0; i < gNbCascades; ++i)
    {
        float2 DxDz = DxDzTextures[uint3(id.xy, i)];
        float2 DyDxz = DyDxzTextures[uint3(id.xy, i)];
        float2 DyxDyz = DyxDyzTextures[uint3(id.xy, i)];
        float2 DxxDzz = DxxDzzTextures[uint3(id.xy, i)];

        displacementsTextures[uint3(id.xy, i)] = float3(DxDz.x, DyDxz.x, DxDz.y);
        derivativesTextures[uint3(id.xy, i)] = float4(DyxDyz, DxxDzz);
        float jacobian = (1 + DxxDzz.x) * (1 + DxxDzz.y) - DyDxz.y * DyDxz.y;
        float foam = turbulenceTextures[uint3(id.xy, i)].x;
        float foamDecayRate = 2;
        foam *= exp(-foamDecayRate);
        if (foam < jacobian)
            foam += jacobian;
        
        turbulenceTextures[uint3(id.xy, i)] = foam;
    }
}
