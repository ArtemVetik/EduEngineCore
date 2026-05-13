static const float PI = 3.14159265;

cbuffer cbConstants : register(b0)
{
    uint gNbCascades;
    uint gTextureSize;
}

cbuffer cbFFTData : register(b1)
{
    uint gTwiddleFactorsAndInputIndicesTextureIdx;
    uint gPingPongTexturesIdx;
}

cbuffer cbFFTPerPass : register(b2)
{
    uint gInputTexturesIdx;
    uint gStep;
    bool gPingPong;
}

// Multiplication of two complex numbers
// https://mathworld.wolfram.com/ComplexMultiplication.html
float2 ComplexMult(float2 a, float2 b)
{
    return float2(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x);
}

// e to the power of a complex number
// e^(x + iy) = e^x * e^iy = e^x * (cos(y) + i*sin(y))
// https://en.wikipedia.org/wiki/Euler%27s_formula
float2 ComplexExp(float2 a)
{
    return exp(a.x) * float2(cos(a.y), sin(a.y));
}

// Computes the "Butterfly Texture", which contains the twiddle factors and input indices required for the "butterfly operations" in the Cooley-Tukey algorithm.
// https://doi.org/10.15480/882.1436 ("4.2.6 Butterfly Texture" section)
// https://en.wikipedia.org/wiki/Butterfly_diagram ("Twiddle Factor" section)
// https://en.wikipedia.org/wiki/Cooley-Tukey_FFT_algorithm
// https://www.tutorialspoint.com/digital_signal_processing/dsp_discrete_time_frequency_transform.htm
// Code source: https://github.com/gasgiant/FFT-Ocean/blob/main/Assets/ComputeShaders/FastFourierTransform.compute
[numthreads(1, 8, 1)]
void PrecomputeTwiddleFactorsAndInputIndices(uint3 id : SV_DispatchThreadID)
{
    RWTexture2D<float4> twiddleFactorsAndInputIndicesTexture = ResourceDescriptorHeap[gTwiddleFactorsAndInputIndicesTextureIdx];
    
    uint b = gTextureSize >> (id.x + 1);
    float2 mult = 2 * PI * float2(0, 1) / gTextureSize;
    uint i = (2 * b * (id.y / b) + id.y % b) % gTextureSize;
    float2 twiddle = ComplexExp(-mult * ((id.y / b) * b));
    twiddleFactorsAndInputIndicesTexture[id.xy] = float4(twiddle.x, twiddle.y, i, i + b);
    twiddleFactorsAndInputIndicesTexture[uint2(id.x, id.y + gTextureSize / 2)] = float4(-twiddle.x, -twiddle.y, i, i + b);
}

// Code source: https://github.com/gasgiant/FFT-Ocean/blob/main/Assets/ComputeShaders/FastFourierTransform.compute
[numthreads(8, 8, 1)]
void HorizontalStepIFFT(uint3 id : SV_DispatchThreadID)
{
    RWTexture2D<float4> twiddleFactorsAndInputIndicesTexture = ResourceDescriptorHeap[gTwiddleFactorsAndInputIndicesTextureIdx];
    RWTexture2DArray<float2> pingPongTextures = ResourceDescriptorHeap[gPingPongTexturesIdx];
    RWTexture2DArray<float2> inputTextures = ResourceDescriptorHeap[gInputTexturesIdx];
    
    float4 data = twiddleFactorsAndInputIndicesTexture[uint2(gStep, id.x)];
    float2 twiddleFactors = float2(data.r, -data.g);
    uint2 inputIndices = (uint2) data.ba;
    for (uint i = 0; i < gNbCascades; ++i)
    {
        if (!gPingPong)
            pingPongTextures[uint3(id.xy, i)] = inputTextures[uint3(inputIndices.x, id.y, i)] + ComplexMult(twiddleFactors, inputTextures[uint3(inputIndices.y, id.y, i)]);
        else
            inputTextures[uint3(id.xy, i)] = pingPongTextures[uint3(inputIndices.x, id.y, i)] + ComplexMult(twiddleFactors, pingPongTextures[uint3(inputIndices.y, id.y, i)]);
    }
}

// Code source: https://github.com/gasgiant/FFT-Ocean/blob/main/Assets/ComputeShaders/FastFourierTransform.compute
[numthreads(8, 8, 1)]
void VerticalStepIFFT(uint3 id : SV_DispatchThreadID)
{
    RWTexture2D<float4> twiddleFactorsAndInputIndicesTexture = ResourceDescriptorHeap[gTwiddleFactorsAndInputIndicesTextureIdx];
    RWTexture2DArray<float2> pingPongTextures = ResourceDescriptorHeap[gPingPongTexturesIdx];
    RWTexture2DArray<float2> inputTextures = ResourceDescriptorHeap[gInputTexturesIdx];
    
    float4 data = twiddleFactorsAndInputIndicesTexture[uint2(gStep, id.y)];
    float2 twiddleFactors = float2(data.r, -data.g);
    uint2 inputIndices = (uint2) data.ba;
    for (uint i = 0; i < gNbCascades; ++i)
    {
        if (!gPingPong)
            pingPongTextures[uint3(id.xy, i)] = inputTextures[uint3(id.x, inputIndices.x, i)] + ComplexMult(twiddleFactors, inputTextures[uint3(id.x, inputIndices.y, i)]);
        else
            inputTextures[uint3(id.xy, i)] = pingPongTextures[uint3(id.x, inputIndices.x, i)] + ComplexMult(twiddleFactors, pingPongTextures[uint3(id.x, inputIndices.y, i)]);
    }
}

// Alternates the sign of each value in the texture based on its position.
// Code source: https://github.com/gasgiant/FFT-Ocean/blob/main/Assets/ComputeShaders/FastFourierTransform.compute
[numthreads(8, 8, 1)]
void Permute(uint3 id : SV_DispatchThreadID)
{
    RWTexture2DArray<float2> inputTextures = ResourceDescriptorHeap[gInputTexturesIdx];
    
    for (uint i = 0; i < gNbCascades; ++i)
    {
        inputTextures[uint3(id.xy, i)] = inputTextures[uint3(id.xy, i)] * (1.0 - 2.0 * ((id.x + id.y) % 2));
    }
}