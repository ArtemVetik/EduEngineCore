cbuffer cbConstants : register(b0)
{
    uint gNbCascades;
    uint gTextureSize;
}

cbuffer cbPassData : register(b1)
{
    float gTime;
}

cbuffer cbInputTextures : register(b2)
{
    uint gDxDzTexturesIdx;                      // RWTexture2DArray<float2>
    uint gDyDxzTexturesIdx;                     // RWTexture2DArray<float2>
    uint gDyxDyzTexturesIdx;                    // RWTexture2DArray<float2>
    uint gDxxDzzTexturesIdx;                    // RWTexture2DArray<float2>
    uint gConjugatedInitialSpectrumTexturesIdx; // Texture2DArray<float4>
    uint gWavesDataTexturesIdx;                 // Texture2DArray<float4>
}

float2 ComplexMult(float2 a, float2 b)
{
    return float2(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x);
}

[numthreads(8, 8, 1)]
void CalculateTimeDependentComplexAmplitudesAndDerivatives(uint3 id : SV_DispatchThreadID)
{
    RWTexture2DArray<float2> DxDzTextures = ResourceDescriptorHeap[gDxDzTexturesIdx];
    RWTexture2DArray<float2> DyDxzTextures = ResourceDescriptorHeap[gDyDxzTexturesIdx];
    RWTexture2DArray<float2> DyxDyzTextures = ResourceDescriptorHeap[gDyxDyzTexturesIdx];
    RWTexture2DArray<float2> DxxDzzTextures = ResourceDescriptorHeap[gDxxDzzTexturesIdx];
    Texture2DArray<float4> conjugatedInitialSpectrumTextures = ResourceDescriptorHeap[gConjugatedInitialSpectrumTexturesIdx];
    Texture2DArray<float4> wavesDataTextures = ResourceDescriptorHeap[gWavesDataTexturesIdx];
    
    for (uint i = 0; i < gNbCascades; ++i)
    {
        float4 wave = wavesDataTextures[uint3(id.xy, i)];
        float phase = wave.w * gTime;
        float2 exponent = float2(cos(phase), sin(phase));
        float2 h = ComplexMult(conjugatedInitialSpectrumTextures[uint3(id.xy, i)].xy, exponent) + ComplexMult(conjugatedInitialSpectrumTextures[uint3(id.xy, i)].zw, float2(exponent.x, -exponent.y));
        float2 ih = float2(-h.y, h.x);

        float2 displacementYDx = ih * wave.x;
        float2 displacementYDz = ih * wave.z;

        float2 displacementX = displacementYDx * wave.y;
        float2 displacementY = h;
        float2 displacementZ = displacementYDz * wave.y;

        float2 aux = -h * wave.y;
        
        float2 displacementXDx = aux * wave.x * wave.x;
        float2 displacementZDz = aux * wave.z * wave.z;
        float2 displacementZDx = aux * wave.x * wave.z;

        DxDzTextures[uint3(id.xy, i)] = float2(displacementX.x - displacementZ.y, displacementX.y + displacementZ.x);
        DyDxzTextures[uint3(id.xy, i)] = float2(displacementY.x - displacementZDx.y, displacementY.y + displacementZDx.x);
        DyxDyzTextures[uint3(id.xy, i)] = float2(displacementYDx.x - displacementYDz.y, displacementYDx.y + displacementYDz.x);
        DxxDzzTextures[uint3(id.xy, i)] = float2(displacementXDx.x - displacementZDz.y, displacementXDx.y + displacementZDz.x);
    }
}
